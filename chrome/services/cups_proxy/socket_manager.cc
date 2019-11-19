// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/socket_manager.h"

#include <errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "chrome/services/cups_proxy/public/cpp/cups_util.h"
#include "chrome/services/cups_proxy/public/cpp/type_conversions.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace cups_proxy {
namespace {

// CUPS daemon socket path
const char kCupsSocketPath[] = "/run/cups/cups.sock";

// Returns true if |response_buffer| represents a full HTTP response.
bool FinishedReadingResponse(const std::vector<uint8_t>& response_buffer) {
  std::string response = ipp_converter::ConvertToString(response_buffer);
  size_t end_of_headers =
      net::HttpUtil::LocateEndOfHeaders(response.data(), response.size());
  if (end_of_headers < 0) {
    return false;
  }

  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      base::StringPiece(response.data(), end_of_headers));
  auto parsed_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);

  // Check that response contains the full body.
  size_t content_length = parsed_headers->GetContentLength();
  if (content_length < 0) {
    return false;
  }

  if (response.size() < end_of_headers + content_length) {
    return false;
  }

  return true;
}

// POD representation of an in-flight socket request.
struct SocketRequest {
  // Explicitly declared/defined defaults since [chromium-style] flagged this as
  // a complex struct.
  SocketRequest();
  SocketRequest(SocketRequest&& other);
  ~SocketRequest();

  // Used for writing/reading the IPP request/response.
  scoped_refptr<net::DrainableIOBuffer> io_buffer;

  std::unique_ptr<std::vector<uint8_t>> response;
  SocketManagerCallback cb;
};

// All methods accessing |socket_| must be made on the IO thread.
// TODO(luum): Consider inner IO-thread object, base::SequenceBound refactor.
class SocketManagerImpl : public SocketManager {
 public:
  explicit SocketManagerImpl(
      std::unique_ptr<net::UnixDomainClientSocket> socket,
      CupsProxyServiceDelegate* const delegate);
  ~SocketManagerImpl() override;

  void ProxyToCups(std::vector<uint8_t> request,
                   SocketManagerCallback cb) override;

 private:
  // These methods support lazily connecting to the CUPS socket.
  void ConnectIfNeeded();
  void Connect();
  void OnConnect(int result);

  void Write();
  void OnWrite(int result);

  void Read();
  void OnRead(int result);

  // Methods for ending a request.
  void Finish(bool success = true);
  void Fail(const char* error_message);

  // Sequence this manager runs on, |in_flight_->cb_| is posted here.
  const scoped_refptr<base::SequencedTaskRunner> main_runner_;

  // Single thread task runner the thread-affine |socket_| runs on.
  const scoped_refptr<base::SingleThreadTaskRunner> socket_runner_;

  // Created in sequence but accessed and deleted on IO thread.
  std::unique_ptr<SocketRequest> in_flight_;
  std::unique_ptr<net::UnixDomainClientSocket> socket_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SocketManagerImpl> weak_factory_{this};
};

// Defaults for SocketRequest.
SocketRequest::SocketRequest() = default;
SocketRequest::SocketRequest(SocketRequest&& other) = default;
SocketRequest::~SocketRequest() = default;

SocketManagerImpl::SocketManagerImpl(
    std::unique_ptr<net::UnixDomainClientSocket> socket,
    CupsProxyServiceDelegate* const delegate)
    : main_runner_(base::SequencedTaskRunnerHandle::Get()),
      socket_runner_(delegate->GetIOTaskRunner()),
      socket_(std::move(socket)) {}
SocketManagerImpl::~SocketManagerImpl() {}

void SocketManagerImpl::ProxyToCups(std::vector<uint8_t> request,
                                    SocketManagerCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!in_flight_);  // Only handles one request at a time.

  // Save request.
  in_flight_ = std::make_unique<SocketRequest>();
  in_flight_->cb = std::move(cb);

  // Fill io_buffer with request to write.
  in_flight_->io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::IOBuffer>(request.size()), request.size());
  std::copy(request.begin(), request.end(), in_flight_->io_buffer->data());

  socket_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&SocketManagerImpl::ConnectIfNeeded,
                                          weak_factory_.GetWeakPtr()));
}

// Separate method since we need to check socket_ on the socket thread.
void SocketManagerImpl::ConnectIfNeeded() {
  DCHECK(socket_runner_->BelongsToCurrentThread());

  // If |socket_| isn't connected yet, connect it.
  if (!socket_->IsConnected()) {
    return Connect();
  }

  // Write request to CUPS.
  return Write();
}

void SocketManagerImpl::Connect() {
  DCHECK(socket_runner_->BelongsToCurrentThread());

  int result = socket_->Connect(base::BindOnce(&SocketManagerImpl::OnConnect,
                                               weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING) {
    return OnConnect(result);
  }
}

void SocketManagerImpl::OnConnect(int result) {
  DCHECK(socket_runner_->BelongsToCurrentThread());
  DCHECK(in_flight_);

  if (result < 0) {
    return Fail("Failed to connect to the CUPS daemon.");
  }

  return Write();
}

void SocketManagerImpl::Write() {
  DCHECK(socket_runner_->BelongsToCurrentThread());

  int result = socket_->Write(
      in_flight_->io_buffer.get(), in_flight_->io_buffer->BytesRemaining(),
      base::BindOnce(&SocketManagerImpl::OnWrite, weak_factory_.GetWeakPtr()),
      TRAFFIC_ANNOTATION_FOR_TESTS /* Unused NetworkAnnotation */);

  if (result != net::ERR_IO_PENDING) {
    return OnWrite(result);
  }
}

void SocketManagerImpl::OnWrite(int result) {
  DCHECK(socket_runner_->BelongsToCurrentThread());
  DCHECK(in_flight_);

  if (result < 0) {
    return Fail("Failed to write IPP request to the CUPS daemon.");
  }

  if (result < in_flight_->io_buffer->BytesRemaining()) {
    in_flight_->io_buffer->DidConsume(result);
    return Write();
  }

  // Prime io_buffer for reading.
  in_flight_->response = std::make_unique<std::vector<uint8_t>>();
  in_flight_->io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::IOBuffer>(kHttpMaxBufferSize),
      kHttpMaxBufferSize);

  // Start reading response from CUPS.
  return Read();
}

void SocketManagerImpl::Read() {
  DCHECK(socket_runner_->BelongsToCurrentThread());

  int result = socket_->Read(
      in_flight_->io_buffer.get(), kHttpMaxBufferSize,
      base::BindOnce(&SocketManagerImpl::OnRead, weak_factory_.GetWeakPtr()));

  if (result != net::ERR_IO_PENDING) {
    return OnRead(result);
  }
}

void SocketManagerImpl::OnRead(int num_read) {
  DCHECK(socket_runner_->BelongsToCurrentThread());
  DCHECK(in_flight_);

  if (num_read < 0) {
    return Fail("Failed to read IPP response from the CUPS daemon.");
  }

  // Save new response data.
  std::copy(in_flight_->io_buffer->data(),
            in_flight_->io_buffer->data() + num_read,
            std::back_inserter(*in_flight_->response));

  // If more response left to read, read more.
  if (!FinishedReadingResponse(*in_flight_->response)) {
    return Read();
  }

  // Finished, got response.
  return Finish();
}

void SocketManagerImpl::Finish(bool success) {
  DCHECK(socket_runner_->BelongsToCurrentThread());

  base::OnceClosure cb;
  if (success) {
    cb = base::BindOnce(std::move(in_flight_->cb),
                        base::Passed(&in_flight_->response));
  } else {
    cb = base::BindOnce(std::move(in_flight_->cb), nullptr);
  }

  // Post callback back to main sequence.
  main_runner_->PostTask(FROM_HERE, std::move(cb));

  // Discard this request while still on IO thread.
  in_flight_.reset();
}

void SocketManagerImpl::Fail(const char* error_message) {
  DVLOG(1) << "SocketManager Error: " << error_message;
  return Finish(false /* Fail this request */);
}

}  // namespace

std::unique_ptr<SocketManager> SocketManager::Create(
    CupsProxyServiceDelegate* const delegate) {
  return std::make_unique<SocketManagerImpl>(
      std::make_unique<net::UnixDomainClientSocket>(
          kCupsSocketPath, false /* not abstract namespace */),
      delegate);
}

std::unique_ptr<SocketManager> SocketManager::CreateForTesting(
    std::unique_ptr<net::UnixDomainClientSocket> socket,
    CupsProxyServiceDelegate* const delegate) {
  return std::make_unique<SocketManagerImpl>(std::move(socket), delegate);
}

}  // namespace cups_proxy
