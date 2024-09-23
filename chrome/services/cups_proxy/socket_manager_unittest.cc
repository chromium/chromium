// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/socket_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/public/cpp/type_conversions.h"
#include "chrome/services/cups_proxy/test/paths.h"
#include "net/base/io_buffer.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

// Returns std::nullopt on failure.
std::optional<std::string> GetTestFile(std::string test_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Build file path.
  base::FilePath path;
  if (!base::PathService::Get(Paths::DIR_TEST_DATA, &path)) {
    return std::nullopt;
  }

  path = path.Append(FILE_PATH_LITERAL(test_name))
             .AddExtension(FILE_PATH_LITERAL(".bin"));

  // Read in file contents.
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    return std::nullopt;
  }

  return contents;
}

}  // namespace

// Fake delegate granting handle to an IO-thread task runner.
class FakeServiceDelegate : public FakeCupsProxyServiceDelegate {
 public:
  FakeServiceDelegate() = default;
  ~FakeServiceDelegate() override = default;

  // Note: Can't simulate actual IO thread in unit_tests, so we serve an
  // arbitrary SingleThreadTaskRunner.
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override {
    return base::ThreadPool::CreateSingleThreadTaskRunner({});
  }
};

// Gives full control over the "CUPS daemon" in this test.
class FakeSocket : public net::UnixDomainClientSocket {
 public:
  FakeSocket() : UnixDomainClientSocket("", false) /* Dummy values */ {}
  ~FakeSocket() override = default;

  // Saves expected request and corresponding response to send back.
  void set_request(std::string_view request) { request_ = request; }
  void set_response(std::string_view response) { response_ = response; }

  // Controls whether each method runs synchronously or asynchronously.
  void set_connect_async() { connect_async = true; }
  void set_read_async() { read_async = true; }
  void set_write_async() { write_async = true; }

  // net::UnixDomainClientSocket overrides.
  bool IsConnected() const override { return is_connected; }

  int Connect(net::CompletionOnceCallback callback) override {
    if (is_connected) {
      // Should've checked IsConnected first.
      return net::ERR_FAILED;
    }

    is_connected = true;

    // Sync
    if (!connect_async) {
      return net::OK;
    }

    // Async
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSocket::OnAsyncCallback, base::Unretained(this),
                       std::move(callback), net::OK));
    return net::ERR_IO_PENDING;
  }

  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    if (!is_connected) {
      return net::ERR_FAILED;
    }

    size_t num_to_read =
        std::min(response_.size(), static_cast<size_t>(buf_len));
    std::copy(response_.begin(), response_.begin() + num_to_read, buf->data());
    response_.remove_prefix(num_to_read);

    // Sync
    if (!read_async) {
      return num_to_read;
    }

    // Async
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSocket::OnAsyncCallback, base::Unretained(this),
                       std::move(callback), num_to_read));
    return net::ERR_IO_PENDING;
  }

  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback,
            const net::NetworkTrafficAnnotationTag& unused) override {
    if (!is_connected) {
      return net::ERR_FAILED;
    }

    // Checks that |buf| holds (part of) the expected request.
    if (!std::equal(buf->data(), buf->data() + buf_len, request_.begin())) {
      return net::ERR_FAILED;
    }

    // Arbitrary maximum write buffer size; just forcing partial writes.
    const size_t kMaxWriteSize = 100;
    size_t num_to_write = std::min(kMaxWriteSize, static_cast<size_t>(buf_len));
    request_.remove_prefix(num_to_write);

    // Sync
    if (!write_async) {
      return num_to_write;
    }

    // Async
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSocket::OnAsyncCallback, base::Unretained(this),
                       std::move(callback), num_to_write));
    return net::ERR_IO_PENDING;
  }

  // Generic callback used to force called methods to return asynchronously.
  void OnAsyncCallback(net::CompletionOnceCallback callback, int net_code) {
    std::move(callback).Run(net_code);
  }

 private:
  bool is_connected = false;
  bool connect_async = false, read_async = false, write_async = false;
  std::string_view request_, response_;
};

class SocketManagerTest : public testing::Test {
 public:
  SocketManagerTest() {
    delegate_ = std::make_unique<FakeServiceDelegate>();

    std::unique_ptr<FakeSocket> socket = std::make_unique<FakeSocket>();
    socket_ = socket.get();

    manager_ =
        SocketManager::CreateForTesting(std::move(socket), delegate_.get());
  }

  std::unique_ptr<std::vector<uint8_t>> ProxyToCups(std::string request) {
    std::vector<uint8_t> request_as_bytes =
        ipp_converter::ConvertToByteBuffer(request);
    std::unique_ptr<std::vector<uint8_t>> response;

    base::RunLoop run_loop;
    manager_->ProxyToCups(std::move(request_as_bytes),
                          base::BindOnce(&SocketManagerTest::OnProxyToCups,
                                         weak_factory_.GetWeakPtr(),
                                         run_loop.QuitClosure(), &response));
    run_loop.Run();
    return response;
  }

 protected:
  // Must be first member.
  base::test::TaskEnvironment task_environment_;

  void OnProxyToCups(base::OnceClosure finish_cb,
                     std::unique_ptr<std::vector<uint8_t>>* ret,
                     std::unique_ptr<std::vector<uint8_t>> result) {
    *ret = std::move(result);
    std::move(finish_cb).Run();
  }

  // Fake injected service delegate.
  std::unique_ptr<FakeServiceDelegate> delegate_;

  // Not owned.
  raw_ptr<FakeSocket> socket_;

  std::unique_ptr<SocketManager> manager_;
  base::WeakPtrFactory<SocketManagerTest> weak_factory_{this};
};

// "basic_handshake" test file contains a simple HTTP request sent by libCUPS,
// copied below for convenience:
//
// POST / HTTP/1.1
// Content-Length: 72
// Content-Type: application/ipp
// Date: Thu, 04 Oct 2018 20:25:59 GMT
// Host: localhost:0
// User-Agent: CUPS/2.3b1 (Linux 4.4.159-15303-g65f4b5a7b3d3; i686) IPP/2.0
//
// @Gattributes-charsetutf-8Hattributes-natural-languageen

// All socket accesses are resolved synchronously.
TEST_F(SocketManagerTest, SyncEverything) {
  // Read request & response
  std::optional<std::string> http_handshake = GetTestFile("basic_handshake");
  EXPECT_TRUE(http_handshake);

  // Pre-load |socket_| with request/response.
  // TODO(crbug.com/41179657): Test with actual http response.
  socket_->set_request(*http_handshake);
  socket_->set_response(*http_handshake);

  auto response = ProxyToCups(*http_handshake);
  EXPECT_TRUE(response);
  EXPECT_EQ(*response, ipp_converter::ConvertToByteBuffer(*http_handshake));
}

TEST_F(SocketManagerTest, AsyncEverything) {
  auto http_handshake = GetTestFile("basic_handshake");
  EXPECT_TRUE(http_handshake);

  socket_->set_request(*http_handshake);
  socket_->set_response(*http_handshake);

  // Set all |socket_| calls to run asynchronously.
  socket_->set_connect_async();
  socket_->set_read_async();
  socket_->set_write_async();

  auto response = ProxyToCups(*http_handshake);
  EXPECT_TRUE(response);
  EXPECT_EQ(*response, ipp_converter::ConvertToByteBuffer(*http_handshake));
}

}  // namespace cups_proxy
