// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file comes from Google Home(cast) implementation.

#include "chromeos/ash/services/libassistant/chromium_http_connection.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using assistant_client::HttpConnection;
using network::PendingSharedURLLoaderFactory;
using network::SharedURLLoaderFactory;

// A macro which ensures we are running in |task_runner_|'s sequence.
#define ENSURE_IN_SEQUENCE(method, ...)                                  \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                     \
    task_runner_->PostTask(FROM_HERE,                                    \
                           base::BindOnce(method, this, ##__VA_ARGS__)); \
    return;                                                              \
  }

namespace ash::libassistant {

namespace {

// Invalid/Unknown HTTP response code.
constexpr int kResponseCodeInvalid = -1;

}  // namespace

ChromiumHttpConnection::ChromiumHttpConnection(
    std::unique_ptr<PendingSharedURLLoaderFactory> pending_url_loader_factory,
    Delegate* delegate)
    : delegate_(delegate),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
      pending_url_loader_factory_(std::move(pending_url_loader_factory)) {
  DCHECK(delegate_);
  DCHECK(pending_url_loader_factory_);

  // Add a reference, so |this| cannot go away until Close() is called.
  AddRef();
}

ChromiumHttpConnection::~ChromiumHttpConnection() {
  // The destructor may be called on another sequence when the connection
  // is cancelled early, for example due to a reconfigure event.
  DCHECK_EQ(state_, State::DESTROYED);
}

void ChromiumHttpConnection::SetRequest(const std::string& url, Method method) {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::SetRequest, url, method);
  DCHECK_EQ(state_, State::NEW);
  url_ = GURL(url);
  method_ = method;
}

void ChromiumHttpConnection::AddHeader(const std::string& name,
                                       const std::string& value) {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::AddHeader, name, value);
  DCHECK_EQ(state_, State::NEW);

  if (!network::IsRequestHeaderSafe(name, value)) {
    VLOG(2) << "Ignoring unsafe request header: " << name;
    return;
  }

  // From https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2:
  // "Multiple message-header fields with the same field-name MAY be present in
  // a message if and only if the entire field-value for that header field is
  // defined as a comma-separated list [i.e., #(values)]. It MUST be possible to
  // combine the multiple header fields into one "field-name: field-value" pair,
  // without changing the semantics of the message, by appending each subsequent
  // field-value to the first, each separated by a comma."
  std::optional<std::string> existing_value = headers_.GetHeader(name);
  if (existing_value) {
    headers_.SetHeader(name, *existing_value + ',' + value);
  } else {
    headers_.SetHeader(name, value);
  }
}

void ChromiumHttpConnection::SetUploadContent(const std::string& content,
                                              const std::string& content_type) {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::SetUploadContent, content,
                     content_type);
  DCHECK_EQ(state_, State::NEW);
  upload_content_ = content;
  upload_content_type_ = content_type;
  chunked_upload_content_type_ = "";
}

void ChromiumHttpConnection::SetChunkedUploadContentType(
    const std::string& content_type) {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::SetChunkedUploadContentType,
                     content_type);
  DCHECK_EQ(state_, State::NEW);
  upload_content_ = "";
  upload_content_type_ = "";
  chunked_upload_content_type_ = content_type;
  AddHeader(::net::HttpRequestHeaders::kContentType, content_type);
}

void ChromiumHttpConnection::EnableHeaderResponse() {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::EnableHeaderResponse)
  enable_header_response_ = true;
}

void ChromiumHttpConnection::EnablePartialResults() {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::EnablePartialResults);
  DCHECK_EQ(state_, State::NEW);
  handle_partial_response_ = true;
}

void ChromiumHttpConnection::Start() {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::Start);
  DCHECK_EQ(state_, State::NEW);
  state_ = State::STARTED;

  if (!url_.is_valid()) {
    state_ = State::COMPLETED;
    VLOG(2) << "Completing connection with invalid URL";
    delegate_->OnNetworkError(kResponseCodeInvalid, "Invalid GURL");
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->headers = headers_;
  switch (method_) {
    case Method::GET:
      resource_request->method = "GET";
      break;
    case Method::POST:
      resource_request->method = "POST";
      break;
    case Method::HEAD:
      resource_request->method = "HEAD";
      break;
    case Method::PATCH:
      resource_request->method = "PATCH";
      break;
    case Method::PUT:
      resource_request->method = "PUT";
      break;
    case Method::DELETE:
      resource_request->method = "DELETE";
      break;
  }
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  const bool chunked_upload =
      !chunked_upload_content_type_.empty() && method_ == Method::POST;
  if (chunked_upload) {
    // Attach a chunked upload body.
    mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> data_remote;
    receiver_set_.Add(this, data_remote.InitWithNewPipeAndPassReceiver());
    resource_request->request_body = new network::ResourceRequestBody();
    resource_request->request_body->SetToChunkedDataPipe(
        std::move(data_remote),
        network::ResourceRequestBody::ReadOnlyOnce(false));
  }

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 NO_TRAFFIC_ANNOTATION_YET);
  url_loader_->SetRetryOptions(
      /*max_retries=*/0, network::SimpleURLLoader::RETRY_NEVER);
  if (!upload_content_type_.empty())
    url_loader_->AttachStringForUpload(upload_content_, upload_content_type_);

  auto factory =
      SharedURLLoaderFactory::Create(std::move(pending_url_loader_factory_));

  if (handle_partial_response_) {
    url_loader_->SetOnResponseStartedCallback(
        base::BindOnce(&ChromiumHttpConnection::OnResponseStarted, this));
    url_loader_->DownloadAsStream(factory.get(), this);
  } else {
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory.get(),
        base::BindOnce(&ChromiumHttpConnection::OnURLLoadComplete, this));
  }
}

void ChromiumHttpConnection::Pause() {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::Pause);
  is_paused_ = true;
}

void ChromiumHttpConnection::Resume() {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::Resume);
  is_paused_ = false;

  if (!partial_response_cache_.empty()) {
    delegate_->OnPartialResponse(partial_response_cache_);
    partial_response_cache_.clear();
  }

  if (on_resume_callback_)
    std::move(on_resume_callback_).Run();
}

void ChromiumHttpConnection::Close() {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::Close);
  if (state_ == State::DESTROYED)
    return;

  state_ = State::DESTROYED;
  url_loader_.reset();

  delegate_->OnConnectionDestroyed();

  Release();
}

void ChromiumHttpConnection::UploadData(const std::string& data,
                                        bool is_last_chunk) {
  ENSURE_IN_SEQUENCE(&ChromiumHttpConnection::UploadData, data, is_last_chunk);
  if (state_ != State::STARTED)
    return;

  upload_body_ += data;

  upload_body_size_ += data.size();
  if (is_last_chunk) {
    // Send size before the rest of the body. While it doesn't matter much, if
    // the other side receives the size before the last chunk, which Mojo does
    // not guarantee, some protocols can merge the data and the last chunk
    // itself into a single frame.
    has_last_chunk_ = is_last_chunk;
    if (get_size_callback_)
      std::move(get_size_callback_).Run(net::OK, upload_body_size_);
  }

  SendData();
}

void ChromiumHttpConnection::GetSize(GetSizeCallback get_size_callback) {
  if (has_last_chunk_)
    std::move(get_size_callback).Run(net::OK, upload_body_size_);
  else
    get_size_callback_ = std::move(get_size_callback);
}

void ChromiumHttpConnection::StartReading(
    mojo::ScopedDataPipeProducerHandle pipe) {
  // Delete any existing pipe, if any.
  upload_pipe_watcher_.reset();
  upload_pipe_ = std::move(pipe);
  upload_pipe_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  upload_pipe_watcher_->Watch(
      upload_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&ChromiumHttpConnection::OnUploadPipeWriteable,
                          base::Unretained(this)));

  // Will attempt to start sending the request body, if any data is available.
  SendData();
}

void ChromiumHttpConnection::OnDataReceived(std::string_view string_piece,
                                            base::OnceClosure resume) {
  DCHECK(handle_partial_response_);

  if (is_paused_) {
    // If the connection is paused, stop sending |OnPartialResponse|
    // notification to the delegate and cache the response part.
    on_resume_callback_ = std::move(resume);
    DCHECK(partial_response_cache_.empty());
    partial_response_cache_ = std::string(string_piece);
  } else {
    DCHECK(partial_response_cache_.empty());
    delegate_->OnPartialResponse(std::string(string_piece));
    std::move(resume).Run();
  }
}

void ChromiumHttpConnection::OnComplete(bool success) {
  DCHECK(handle_partial_response_);

  if (state_ != State::STARTED)
    return;

  state_ = State::COMPLETED;

  int response_code = kResponseCodeInvalid;
  std::string raw_headers;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    raw_headers = url_loader_->ResponseInfo()->headers->raw_headers();
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  if (response_code != kResponseCodeInvalid) {
    delegate_->OnCompleteResponse(response_code, raw_headers, /*response=*/"");
    return;
  }

  const std::string message = net::ErrorToString(url_loader_->NetError());
  VLOG(3) << "ChromiumHttpConnection completed with network error="
          << url_loader_->NetError() << ": " << message;
  delegate_->OnNetworkError(url_loader_->NetError(), message);
}

void ChromiumHttpConnection::OnRetry(base::OnceClosure start_retry) {
  DCHECK(handle_partial_response_);
  // Retries are not enabled for these requests.
  NOTREACHED_IN_MIGRATION();
}

// Attempts to send more of the upload body, if more data is available, and
// |upload_pipe_| is valid.
void ChromiumHttpConnection::SendData() {
  if (!upload_pipe_.is_valid() || upload_body_.empty()) {
    return;
  }

  size_t bytes_written = 0;
  MojoResult result =
      upload_pipe_->WriteData(base::as_byte_span(upload_body_),
                              MOJO_WRITE_DATA_FLAG_NONE, bytes_written);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // Wait for the pipe to have more capacity available.
    upload_pipe_watcher_->ArmOrNotify();
    return;
  }

  // Depend on |url_loader_| to notice the other pipes being closed on error.
  if (result != MOJO_RESULT_OK)
    return;

  upload_body_.erase(0, bytes_written);

  // If more data is available, arm the watcher again. Don't write again in a
  // loop, even if WriteData would allow it, to avoid blocking the current
  // thread.
  if (!upload_body_.empty()) {
    upload_pipe_watcher_->ArmOrNotify();
  }
}

void ChromiumHttpConnection::OnUploadPipeWriteable(MojoResult unused) {
  SendData();
}

void ChromiumHttpConnection::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(!handle_partial_response_);

  if (state_ != State::STARTED)
    return;

  state_ = State::COMPLETED;

  if (url_loader_->NetError() != net::OK) {
    delegate_->OnNetworkError(kResponseCodeInvalid,
                              net::ErrorToString(url_loader_->NetError()));
    return;
  }

  int response_code = kResponseCodeInvalid;
  std::string raw_headers;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    raw_headers = url_loader_->ResponseInfo()->headers->raw_headers();
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  if (response_code == kResponseCodeInvalid) {
    std::string message = net::ErrorToString(url_loader_->NetError());

    VLOG(3) << "ChromiumHttpConnection completed with network error="
            << response_code << ": " << message;
    delegate_->OnNetworkError(response_code, message);
    return;
  }

  VLOG(3) << "ChromiumHttpConnection completed with response_code="
          << response_code;

  delegate_->OnCompleteResponse(response_code, raw_headers, *response_body);
}

void ChromiumHttpConnection::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_header) {
  if (enable_header_response_ && response_header.headers) {
    // Only propagate |OnHeaderResponse()| once before any |OnPartialResponse()|
    // invoked to honor the API contract.
    delegate_->OnHeaderResponse(response_header.headers->raw_headers());
  }
}

ChromiumHttpConnectionFactory::ChromiumHttpConnectionFactory(
    std::unique_ptr<PendingSharedURLLoaderFactory> pending_url_loader_factory)
    : url_loader_factory_(SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory))) {}

ChromiumHttpConnectionFactory::~ChromiumHttpConnectionFactory() = default;

HttpConnection* ChromiumHttpConnectionFactory::Create(
    HttpConnection::Delegate* delegate) {
  return new ChromiumHttpConnection(url_loader_factory_->Clone(), delegate);
}

}  // namespace ash::libassistant
