// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/android_stream_reader_url_loader.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/trace_event/base_tracing.h"
#include "components/embedder_support/android/util/features.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "components/embedder_support/android/util/input_stream_reader.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace embedder_support {

namespace {

const char kHTTPOkText[] = "OK";
const char kHTTPNotFoundText[] = "Not Found";

const int kMaxBytesToReadWhenAvailableUnknown = 2 * 1024;

}  // namespace

namespace {

using OnInputStreamOpenedCallback = base::OnceCallback<void(
    std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>,
    std::unique_ptr<InputStream>)>;

// static
void OpenInputStreamOnWorkerThread(
    scoped_refptr<base::SingleThreadTaskRunner> job_thread_task_runner,
    std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate> delegate,
    OnInputStreamOpenedCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  std::unique_ptr<InputStream> input_stream = delegate->OpenInputStream(env);

  job_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(delegate),
                                std::move(input_stream)));
}

network::ResourceRequest CopyResourceRequest(
    const network::ResourceRequest& request) {
  // If the features is disabled, copy the full request to preserve previous
  // behavior.
  if (!base::FeatureList::IsEnabled(
          network::features::kAvoidResourceRequestCopies)) {
    return request;
  }

  // Copy only the fields we need from the request.
  network::ResourceRequest new_request;
  new_request.url = request.url;
  new_request.mode = request.mode;
  new_request.headers = request.headers;
  return new_request;
}

}  // namespace

// In the case when stream reader related tasks are posted on a dedicated
// thread they can outlive the loader. This is a wrapper is for holding both
// InputStream and InputStreamReader to ensure they are still there when the
// task is run.
class InputStreamReaderWrapper
    : public base::RefCountedThreadSafe<InputStreamReaderWrapper> {
 public:
  InputStreamReaderWrapper(
      std::unique_ptr<InputStream> input_stream,
      std::unique_ptr<InputStreamReader> input_stream_reader)
      : input_stream_(std::move(input_stream)),
        input_stream_reader_(std::move(input_stream_reader)) {
    DCHECK(input_stream_);
    DCHECK(input_stream_reader_);
  }

  InputStreamReaderWrapper(const InputStreamReaderWrapper&) = delete;
  InputStreamReaderWrapper& operator=(const InputStreamReaderWrapper&) = delete;

  InputStream* input_stream() { return input_stream_.get(); }

  int Seek(const net::HttpByteRange& byte_range) {
    return input_stream_reader_->Seek(byte_range);
  }

  int ReadRawData(net::IOBuffer* buffer, int buffer_size) {
    int available = 0;
    // Only use `available` if the app has an estimate, otherwise it'll return
    // 0. In that case we still want to do a blocking read until there's data
    // or EOF. Note some implementations return 1 to indicate there's more data.
    if (input_stream_->BytesAvailable(&available) && available > 1) {
      // Make sure a we don't read past the buffer size.
      buffer_size = std::min(available, buffer_size);
    } else {
      // `buffer_size' could be large since it comes from the size of the data
      // pipe, but we don't want to synchronously wait for too many bytes in
      // case they're coming from the network.
      buffer_size = std::min(kMaxBytesToReadWhenAvailableUnknown, buffer_size);
    }

    return input_stream_reader_->ReadRawData(buffer, buffer_size);
  }

 private:
  friend class base::RefCountedThreadSafe<InputStreamReaderWrapper>;
  ~InputStreamReaderWrapper() = default;

  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<InputStreamReader> input_stream_reader_;
};

bool AndroidStreamReaderURLLoader::ResponseDelegate::ShouldCacheResponse(
    network::mojom::URLResponseHead* response) {
  return false;
}

AndroidStreamReaderURLLoader::AndroidStreamReaderURLLoader(
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    std::unique_ptr<ResponseDelegate> response_delegate,
    std::optional<SecurityOptions> security_options,
    std::optional<SetCookieHeader> set_cookie_header)
    : resource_request_(CopyResourceRequest(resource_request)),
      response_head_(network::mojom::URLResponseHead::New()),
      reject_cors_request_(false),
      client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      response_delegate_(std::move(response_delegate)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunner::GetCurrentDefault()),
      start_time_(base::Time::Now()),
      set_cookie_header_(set_cookie_header) {
  DCHECK(response_delegate_);
  // If there is a client error, clean up the request.
  client_.set_disconnect_handler(
      base::BindOnce(&AndroidStreamReaderURLLoader::RequestComplete,
                     weak_factory_.GetWeakPtr(), net::ERR_ABORTED));

  bool is_request_considered_same_origin = false;
  if (security_options) {
    DCHECK(!security_options->allow_cors_to_same_scheme ||
           resource_request.request_initiator);
    is_request_considered_same_origin =
        security_options->disable_web_security ||
        (security_options->allow_cors_to_same_scheme &&
         resource_request.request_initiator->IsSameOriginWith(
             resource_request_.url));
    reject_cors_request_ = true;
  }
  response_head_->response_type = network::cors::CalculateResponseType(
      resource_request_.mode, is_request_considered_same_origin);
}

AndroidStreamReaderURLLoader::~AndroidStreamReaderURLLoader() = default;

void AndroidStreamReaderURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {}
void AndroidStreamReaderURLLoader::SetPriority(net::RequestPriority priority,
                                               int intra_priority_value) {}
void AndroidStreamReaderURLLoader::PauseReadingBodyFromNet() {}
void AndroidStreamReaderURLLoader::ResumeReadingBodyFromNet() {}

void AndroidStreamReaderURLLoader::Start(
    std::unique_ptr<InputStream> input_stream) {
  TRACE_EVENT0("android_webview", "AndroidStreamReaderURLLoader::Start");
  DCHECK(thread_checker_.CalledOnValidThread());

  if (reject_cors_request_ && response_head_->response_type ==
                                  network::mojom::FetchResponseType::kCors) {
    RequestCompleteWithStatus(
        network::URLLoaderCompletionStatus(network::CorsErrorStatus(
            network::mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  if (!ParseRange(resource_request_.headers)) {
    RequestComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  if (input_stream) {
    OnInputStreamOpened(std::move(response_delegate_), std::move(input_stream));
  } else {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            &OpenInputStreamOnWorkerThread,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            // This is intentional - the loader could be deleted while the
            // callback is executing on the background thread. The delegate will
            // be "returned" to the loader once the InputStream open attempt is
            // completed.
            std::move(response_delegate_),
            base::BindOnce(&AndroidStreamReaderURLLoader::OnInputStreamOpened,
                           weak_factory_.GetWeakPtr())));
  }
}

void AndroidStreamReaderURLLoader::OnInputStreamOpened(
    std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>
        returned_delegate,
    std::unique_ptr<InputStream> input_stream) {
  TRACE_EVENT0("android_webview",
               "AndroidStreamReaderURLLoader::OnInputStreamOpened");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(returned_delegate);
  response_delegate_ = std::move(returned_delegate);

  if (!input_stream) {
    bool restarted_or_completed = response_delegate_->OnInputStreamOpenFailed();
    if (restarted_or_completed) {
      // The original request has been restarted with a new loader or
      // completed. We can clean up this loader.
      // Generally speaking this can happen in the following cases
      // (see aw_proxying_url_loader_factory.cc for delegate implementation):
      //   1. InterceptResponseDelegate :
      //     - intercepted requests with custom response,
      //     - no restart required
      //   2. ProtocolResponseDelegate :
      //     - e.g. file:///android_asset/,
      //     - restart required
      //   3. InterceptResponseDelegate, intercept-only setting :
      //     - used for external protocols,
      //     - no restart, but completes immediately.
      CleanUp();
    } else {
      HeadersComplete(net::HTTP_NOT_FOUND, kHTTPNotFoundText);
    }
    return;
  }

  auto input_stream_reader =
      std::make_unique<InputStreamReader>(input_stream.get());
  DCHECK(input_stream);
  DCHECK(!input_stream_reader_wrapper_);

  input_stream_reader_wrapper_ = base::MakeRefCounted<InputStreamReaderWrapper>(
      std::move(input_stream), std::move(input_stream_reader));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&InputStreamReaderWrapper::Seek,
                     input_stream_reader_wrapper_, byte_range_),
      base::BindOnce(&AndroidStreamReaderURLLoader::OnReaderSeekCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLLoader::OnReaderSeekCompleted(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (result >= 0) {
    // we've got the expected content size here
    expected_content_size_ = result;
    HeadersComplete(net::HTTP_OK, kHTTPOkText);
  } else {
    RequestComplete(net::ERR_FAILED);
  }
}

void AndroidStreamReaderURLLoader::HeadersComplete(
    int status_code,
    const std::string& status_text) {
  TRACE_EVENT0("android_webview",
               "AndroidStreamReaderURLLoader::HeadersComplete");
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string status("HTTP/1.1 ");
  status.append(base::NumberToString(status_code));
  status.append(" ");
  status.append(status_text);
  // HttpResponseHeaders expects its input string to be terminated by two NULs.
  status.append("\0\0", 2);

  network::mojom::URLResponseHead& head = *response_head_;
  head.request_start = base::TimeTicks::Now();
  head.response_start = base::TimeTicks::Now();
  head.headers = new net::HttpResponseHeaders(status);

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  if (status_code == net::HTTP_OK) {
    response_delegate_->GetCharset(env, resource_request_.url,
                                   input_stream_reader_wrapper_->input_stream(),
                                   &head.charset);

    if (expected_content_size_ != -1) {
      head.headers->SetHeader(net::HttpRequestHeaders::kContentLength,
                              base::NumberToString(expected_content_size_));
      head.content_length = expected_content_size_;
    }

    std::string mime_type;
    if (response_delegate_->GetMimeType(
            env, resource_request_.url,
            input_stream_reader_wrapper_->input_stream(), &mime_type) &&
        !mime_type.empty()) {
      head.headers->SetHeader(net::HttpRequestHeaders::kContentType, mime_type);
      head.mime_type = mime_type;
    }
  }

  response_delegate_->AppendResponseHeaders(env, head.headers.get());

  SendBody();
}

void AndroidStreamReaderURLLoader::SendBody() {
  DCHECK(thread_checker_.CalledOnValidThread());

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize(
          network::features::DataPipeAllocationSize::kLargerSizeIfPossible);
  if (CreateDataPipe(&options, producer_handle_, consumer_handle_) !=
      MOJO_RESULT_OK) {
    RequestComplete(net::ERR_FAILED);
    return;
  }
  writable_handle_watcher_.Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&AndroidStreamReaderURLLoader::OnDataPipeWritable,
                          base::Unretained(this)));

  // Send the response if possible now for 2 reasons:
  // 1. If we don't need any more MIME type sniffing, there's no reason not to
  //    tell the URLLoaderClient right away. Sending now should preserve
  //    ordering between app-visible callbacks and the first read of the
  //    InputStream (although we do not generally guarantee the ordering).
  // 2. Sending this now lets us unittest the net::ERR_ABORTED case. The case
  //    needs the ability to break the stream after getting the headers but
  //    before finishing the read.
  if (!response_head_->mime_type.empty()) {
    SendResponseToClient();
  }
  ReadMore();
}

void AndroidStreamReaderURLLoader::SetCookies() {
  if (!set_cookie_header_.has_value()) {
    return;
  }

  const std::string_view kSetCookieHeader("Set-Cookie");

  if (response_head_->headers->HasHeader(kSetCookieHeader)) {
    std::optional<base::Time> server_time =
        response_head_->headers->GetDateValue();

    std::string cookie_string;
    size_t iter = 0;

    while (response_head_->headers->EnumerateHeader(&iter, kSetCookieHeader,
                                                    &cookie_string)) {
      std::move(set_cookie_header_)
          ->Run(resource_request_, cookie_string, server_time);
    }
  }
}

void AndroidStreamReaderURLLoader::SendResponseToClient() {
  DCHECK(consumer_handle_.is_valid());
  DCHECK(client_.is_bound());
  SetCookies();
  cache_response_ =
      response_delegate_->ShouldCacheResponse(response_head_.get());
  client_->OnReceiveResponse(std::move(response_head_),
                             std::move(consumer_handle_), std::nullopt);
}

void AndroidStreamReaderURLLoader::ReadMore() {
  TRACE_EVENT0("android_webview", "AndroidStreamReaderURLLoader::ReadMore");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!pending_buffer_.get());

  MojoResult mojo_result = network::NetToMojoPendingBuffer::BeginWrite(
      &producer_handle_, &pending_buffer_);
  switch (mojo_result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      // The pipe is full. We need to wait for it to have more space.
      writable_handle_watcher_.ArmOrNotify();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data pipe consumer handle has been closed.
      RequestComplete(net::ERR_ABORTED);
      return;
    default:
      // The body stream is in a bad state. Bail out.
      RequestComplete(net::ERR_UNEXPECTED);
      return;
  }
  uint32_t num_bytes = pending_buffer_->size();
  auto buffer =
      base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_buffer_);

  if (!input_stream_reader_wrapper_.get()) {
    // This will happen if opening the InputStream fails in which case the
    // error is communicated by setting the HTTP response status header rather
    // than failing the request during the header fetch phase.
    DidRead(0);
    return;
  }

  // TODO(timvolodine): consider using a sequenced task runner.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &InputStreamReaderWrapper::ReadRawData, input_stream_reader_wrapper_,
          base::RetainedRef(buffer.get()), base::checked_cast<int>(num_bytes)),
      base::BindOnce(&AndroidStreamReaderURLLoader::DidRead,
                     weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLLoader::DidRead(int result) {
  TRACE_EVENT1("android_webview", "AndroidStreamReaderURLLoader::DidRead",
               "bytes_read", result);
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(pending_buffer_);
  if (result < 0) {
    // error case
    RequestComplete(result);
    return;
  }
  if (result == 0) {
    // eof, read completed
    pending_buffer_->Complete(0);
    RequestComplete(net::OK);
    return;
  }
  if (consumer_handle_.is_valid()) {
    // We only hit this on for the first buffer read, which we expect to be
    // enough to determine the MIME type.
    if (response_head_->mime_type.empty()) {
      // Limit sniffing to the first net::kMaxBytesToSniff.
      size_t data_length = result;
      if (data_length > net::kMaxBytesToSniff)
        data_length = net::kMaxBytesToSniff;

      std::string new_type;
      net::SniffMimeType(
          std::string_view(pending_buffer_->buffer(), data_length),
          resource_request_.url, std::string(),
          net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
      // SniffMimeType() returns false if there is not enough data to
      // determine the mime type. However, even if it returns false, it
      // returns a new type that is probably better than the current one.
      response_head_->mime_type.assign(new_type);
      response_head_->did_mime_sniff = true;
    }

    SendResponseToClient();
  }

  if (cache_response_)
    cached_response_.append(pending_buffer_->buffer(), result);

  producer_handle_ = pending_buffer_->Complete(result);
  pending_buffer_ = nullptr;

  if (base::FeatureList::IsEnabled(features::kInputStreamOptimizations)) {
    ReadMore();
  } else {
    // TODO(timvolodine): consider using a sequenced task runner.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AndroidStreamReaderURLLoader::ReadMore,
                                  weak_factory_.GetWeakPtr()));
  }
}

void AndroidStreamReaderURLLoader::OnDataPipeWritable(MojoResult result) {
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    RequestComplete(net::ERR_ABORTED);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK) << result;

  ReadMore();
}

void AndroidStreamReaderURLLoader::RequestCompleteWithStatus(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0("android_webview",
               "AndroidStreamReaderURLLoader::RequestCompleteWithStatus");
  DCHECK(thread_checker_.CalledOnValidThread());
  if (consumer_handle_.is_valid()) {
    // We can hit this before reading any buffers under error conditions.
    SendResponseToClient();
  }

  client_->OnComplete(status);
  UMA_HISTOGRAM_TIMES("Android.WebView.InputStreamTime",
                      base::Time::Now() - start_time_);
  CleanUp();
}

void AndroidStreamReaderURLLoader::RequestComplete(int status_code) {
  if (status_code == net::OK && cache_response_)
    response_delegate_->OnResponseCache(cached_response_);

  RequestCompleteWithStatus(network::URLLoaderCompletionStatus(status_code));
}

void AndroidStreamReaderURLLoader::CleanUp() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Resets the watchers and pipes, so that we will never be called back.
  writable_handle_watcher_.Cancel();
  pending_buffer_ = nullptr;
  producer_handle_.reset();

  // Manages its own lifetime
  delete this;
}

// TODO(timvolodine): consider moving this to the net_helpers.cc
bool AndroidStreamReaderURLLoader::ParseRange(
    const net::HttpRequestHeaders& headers) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::optional<std::string> range_header =
      headers.GetHeader(net::HttpRequestHeaders::kRange);
  if (range_header) {
    // This loader only cares about the Range header so that we know how many
    // bytes in the stream to skip and how many to read after that.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(*range_header, &ranges)) {
      // In case of multi-range request only use the first range.
      // We don't support multirange requests.
      if (ranges.size() == 1)
        byte_range_ = ranges[0];
    } else {
      // This happens if the range header could not be parsed or is invalid.
      return false;
    }
  }
  return true;
}

}  // namespace embedder_support
