// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

#import "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/event_logger.h"
#include "chrome/updater/net/chunk_queue.h"
#include "chrome/updater/net/fallback_net_fetcher.h"
#include "chrome/updater/net/fetcher_callback_adapter.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/net/network_file_fetcher.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/protos/omaha_usage_stats_event.pb.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/wait.h"
#import "net/base/apple/url_conversions.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

using ResponseStartedCallback =
    ::update_client::NetworkFetcher::ResponseStartedCallback;
using ProgressCallback = ::update_client::NetworkFetcher::ProgressCallback;
using PostRequestCompleteCallback =
    ::update_client::NetworkFetcher::PostRequestCompleteCallback;
using DownloadCompleteCallback =
    ::update_client::NetworkFetcher::DownloadToFileCompleteCallback;

@interface CRUUpdaterNetworkController : NSObject <NSURLSessionDelegate>
- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                               progressCallback:
                                   (ProgressCallback)progressCallback;
@end

@implementation CRUUpdaterNetworkController {
 @protected
  ResponseStartedCallback _responseStartedCallback;
  ProgressCallback _progressCallback;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                               progressCallback:
                                   (ProgressCallback)progressCallback {
  if (self = [super init]) {
    _responseStartedCallback = std::move(responseStartedCallback);
    _progressCallback = progressCallback;
    _callbackRunner = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return self;
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  if (error) {
    DLOG(ERROR) << "NSURLSession error: " << error
                << ". NSURLSession: " << session
                << ". NSURLSessionTask: " << task;
  }
}
@end

@interface CRUUpdaterNetworkDataDelegate
    : CRUUpdaterNetworkController <NSURLSessionDataDelegate>
- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
        postRequestCompleteCallback:
            (PostRequestCompleteCallback)postRequestCompleteCallback;
@end

@implementation CRUUpdaterNetworkDataDelegate {
  PostRequestCompleteCallback _postRequestCompleteCallback;
  NSMutableData* __strong _downloadedData;
}

- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
        postRequestCompleteCallback:
            (PostRequestCompleteCallback)postRequestCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:progressCallback]) {
    _postRequestCompleteCallback = std::move(postRequestCompleteCallback);
    _downloadedData = [[NSMutableData alloc] init];
  }
  return self;
}

#pragma mark - NSURLSessionDataDelegate

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  [_downloadedData appendData:data];
  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(_progressCallback, dataTask.countOfBytesReceived));
  [dataTask resume];
}

// Tells the delegate that the data task received the initial reply (headers)
// from the server.
- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:
         (void (^)(NSURLSessionResponseDisposition))completionHandler {
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(std::move(_responseStartedCallback),
                                [(NSHTTPURLResponse*)response statusCode],
                                dataTask.countOfBytesExpectedToReceive));
  if (completionHandler) {
    completionHandler(NSURLSessionResponseAllow);
  }
  [dataTask resume];
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [super URLSession:session task:task didCompleteWithError:error];

  NSHTTPURLResponse* response = (NSHTTPURLResponse*)task.response;
  NSDictionary* headers = response.allHeaderFields;

  NSString* headerEtag =
      base::SysUTF8ToNSString(update_client::NetworkFetcher::kHeaderEtag);
  NSString* etag = @"";
  if ([headers objectForKey:headerEtag]) {
    etag = [headers objectForKey:headerEtag];
  }
  NSString* headerXCupServerProof = base::SysUTF8ToNSString(
      update_client::NetworkFetcher::kHeaderXCupServerProof);
  NSString* cupServerProof = @"";
  if ([headers objectForKey:headerXCupServerProof]) {
    cupServerProof = [headers objectForKey:headerXCupServerProof];
  }
  NSString* headerSetCookie =
      base::SysUTF8ToNSString(update_client::NetworkFetcher::kHeaderSetCookie);
  NSString* setCookie = @"";
  if ([headers objectForKey:headerSetCookie]) {
    setCookie = [headers objectForKey:headerSetCookie];
  }

  int64_t retryAfterResult = -1;
  NSString* xRetryAfter = [headers
      objectForKey:base::SysUTF8ToNSString(
                       update_client::NetworkFetcher::kHeaderXRetryAfter)];
  if (xRetryAfter) {
    retryAfterResult = [xRetryAfter intValue];
  }

  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(_postRequestCompleteCallback),
          std::string(reinterpret_cast<const char*>([_downloadedData bytes]),
                      [_downloadedData length]),
          error.code, base::SysNSStringToUTF8(etag),
          base::SysNSStringToUTF8(cupServerProof),
          base::SysNSStringToUTF8(setCookie), retryAfterResult));
}

@end

@interface CRUUpdaterNetworkDownloadDelegate
    : CRUUpdaterNetworkController <NSURLSessionDownloadDelegate>
- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                               progressCallback:
                                   (ProgressCallback)progressCallback
                                       filePath:(const base::FilePath&)filePath
                       downloadCompleteCallback:
                           (DownloadCompleteCallback)downloadCompleteCallback;
@end

@implementation CRUUpdaterNetworkDownloadDelegate {
  base::FilePath _filePath;
  bool _moveTempFileSuccessful;
  DownloadCompleteCallback _downloadCompleteCallback;
}

- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                               progressCallback:
                                   (ProgressCallback)progressCallback
                                       filePath:(const base::FilePath&)filePath
                       downloadCompleteCallback:
                           (DownloadCompleteCallback)downloadCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:progressCallback]) {
    _filePath = filePath;
    _moveTempFileSuccessful = false;
    _downloadCompleteCallback = std::move(downloadCompleteCallback);
  }
  return self;
}

#pragma mark - NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession*)session
             dataTask:(NSURLSessionDataTask*)dataTask
    willCacheResponse:(NSCachedURLResponse*)proposedResponse
    completionHandler:
        (void (^)(NSCachedURLResponse* _Nullable))completionHandler {
  completionHandler(nullptr);
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  if (!location) {
    return;
  }

  const base::FilePath tempPath =
      base::apple::NSStringToFilePath([location path]);
  _moveTempFileSuccessful = base::Move(tempPath, _filePath);
  if (!_moveTempFileSuccessful) {
    DPLOG(ERROR)
        << "Failed to move the downloaded file from the temporary location: "
        << tempPath << " to: " << _filePath;
  }
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [super URLSession:session task:task didCompleteWithError:error];

  NSInteger result;

  if (error) {
    result = [error code];
    DLOG(ERROR) << "NSError code: " << result << ". NSErrorDomain: "
                << base::SysNSStringToUTF8([error domain])
                << ". NSError description: "
                << base::SysNSStringToUTF8([error description]);
  } else {
    NSHTTPURLResponse* response = (NSHTTPURLResponse*)task.response;
    result = response.statusCode == 200 ? 0 : response.statusCode;

    if (!result && !_moveTempFileSuccessful) {
      DLOG(ERROR) << "File downloaded successfully. Moving temp file failed.";
      result = updater::kErrorFailedToMoveDownloadedFile;
    }
  }

  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(std::move(_downloadCompleteCallback), result,
                                [task countOfBytesReceived]));
}

@end

namespace updater {

// DownloadStreamWriter receives bytes from a NSURLSessionTask and pushes them
// to a ScopedDataPipeProducerHandle, applying back-pressure by suspending the
// download task if the pipe is full.
class DownloadStreamWriter {
 public:
  DownloadStreamWriter(mojo::ScopedDataPipeProducerHandle destination,
                       DownloadCompleteCallback download_complete_callback)
      : destination_(std::move(destination)),
        completion_callback_(std::move(download_complete_callback)),
        callback_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                 callback_runner_) {}

  DownloadStreamWriter(const DownloadStreamWriter&) = delete;
  DownloadStreamWriter& operator=(const DownloadStreamWriter&) = delete;

  ~DownloadStreamWriter() = default;

  void OnDataReceived(NSURLSessionDataTask* data_task, NSData* data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(data_task);

    // Because data is posted from the NSURLSession's queue to a sequence,
    // chunks may be received even after the pipe has been closed due to a write
    // error. Refuse chunks if the handle has been reset.
    if (!destination_->is_valid()) {
      return;
    }

    BOOL was_empty = pending_data_.empty();
    pending_data_.Push(data);

    if (was_empty) {
      WritePendingData(data_task);
    }
  }

  void OnNetworkComplete(NSURLSessionTask* task, NSError* error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(task);

    NSInteger result = 0;
    if (error) {
      result = error.code;
      VLOG(1) << "NSError code: " << result
              << ". NSErrorDomain: " << base::SysNSStringToUTF8(error.domain)
              << ". NSError description: "
              << base::SysNSStringToUTF8(error.description);
    } else {
      NSHTTPURLResponse* response = (NSHTTPURLResponse*)task.response;
      CHECK(response);
      NSInteger statusCode = response.statusCode;
      result = statusCode == 200 ? 0 : response.statusCode;
    }

    completed_ = true;
    final_result_ = result;
    final_bytes_received_ = task.countOfBytesReceived;

    if (pending_data_.empty()) {
      CompleteDownload();
    }
  }

 private:
  void StartWatching(NSURLSessionDataTask* data_task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(data_task);
    if (!watcher_.IsWatching()) {
      watcher_.Watch(destination_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                     base::BindRepeating(&DownloadStreamWriter::OnPipeWritable,
                                         base::Unretained(this), data_task));
    }
    watcher_.ArmOrNotify();
  }

  MojoResult WriteDataToPipe(base::span<const uint8_t> slice,
                             size_t& bytes_written) {
    return destination_->WriteData(slice, MOJO_WRITE_DATA_FLAG_NONE,
                                   bytes_written);
  }

  void WritePendingData(NSURLSessionDataTask* data_task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(data_task);

    MojoResult result = pending_data_.Consume(base::BindRepeating(
        &DownloadStreamWriter::WriteDataToPipe, base::Unretained(this)));

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      [data_task suspend];
      StartWatching(data_task);
      return;
    }

    if (result != MOJO_RESULT_OK) {
      [data_task cancel];
      OnMojoError();
      return;
    }

    watcher_.Cancel();
    if (completed_ && pending_data_.empty()) {
      CompleteDownload();
      return;
    }
    [data_task resume];
  }

  void OnPipeWritable(NSURLSessionDataTask* data_task,
                      MojoResult result,
                      const mojo::HandleSignalsState& state) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(data_task);
    if (result != MOJO_RESULT_OK) {
      [data_task cancel];
      OnMojoError();
      return;
    }
    WritePendingData(data_task);
  }

  void OnMojoError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    destination_.reset();
    watcher_.Cancel();
    if (completion_callback_) {
      callback_runner_->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(completion_callback_),
                                                kErrorFailedToWriteFile, -1));
    }
  }

  void CompleteDownload() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    destination_.reset();
    watcher_.Cancel();
    // The completion callback may have already been posted if the pipe was
    // closed due to a write error via `OnMojoError`. This function may still be
    // reachable as canceling an NSURLDataSessionTask will invoke
    // `URLSession:task:didCompleteWithError:`.
    if (completion_callback_) {
      callback_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(completion_callback_),
                                    final_result_, final_bytes_received_));
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::ScopedDataPipeProducerHandle destination_;
  DownloadCompleteCallback completion_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  mojo::SimpleWatcher watcher_;

  ChunkQueue pending_data_;
  bool completed_ = false;
  NSInteger final_result_ = 0;
  int64_t final_bytes_received_ = 0;
};

}  // namespace updater

@interface CRUUpdaterNetworkStreamDelegate
    : CRUUpdaterNetworkController <NSURLSessionDataDelegate>
- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                                    destination:
                                        (mojo::ScopedDataPipeProducerHandle)
                                            destination
                       downloadCompleteCallback:
                           (DownloadCompleteCallback)downloadCompleteCallback;
@end

@implementation CRUUpdaterNetworkStreamDelegate {
  base::SequenceBound<updater::DownloadStreamWriter> _writer;
}

- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                                    destination:
                                        (mojo::ScopedDataPipeProducerHandle)
                                            destination
                       downloadCompleteCallback:
                           (DownloadCompleteCallback)downloadCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:base::DoNothing()]) {
    _writer = base::SequenceBound<updater::DownloadStreamWriter>(
        _callbackRunner, std::move(destination),
        std::move(downloadCompleteCallback));
  }
  return self;
}

#pragma mark - NSURLSessionDataDelegate

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  _writer.AsyncCall(&updater::DownloadStreamWriter::OnDataReceived)
      .WithArgs(dataTask, [data copy]);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:
         (void (^)(NSURLSessionResponseDisposition))completionHandler {
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(std::move(_responseStartedCallback),
                                [(NSHTTPURLResponse*)response statusCode],
                                dataTask.countOfBytesExpectedToReceive));
  if (completionHandler) {
    completionHandler(NSURLSessionResponseAllow);
  }
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [super URLSession:session task:task didCompleteWithError:error];

  _writer.AsyncCall(&updater::DownloadStreamWriter::OnNetworkComplete)
      .WithArgs(task, error);
}

@end

namespace updater {
namespace {

// DownloadStreamReader receives a bytes from a ScopedDataPipeConsumerHandle and
// pushes them to a file.
class DownloadStreamReader : public mojo::DataPipeDrainer::Client {
 public:
  DownloadStreamReader(mojo::ScopedDataPipeConsumerHandle consumer,
                       const base::FilePath& file_path,
                       ProgressCallback progress_callback,
                       DownloadCompleteCallback download_complete_callback)
      : file_(file_path,
              base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                  base::File::FLAG_NO_FOLLOW),
        progress_callback_(progress_callback),
        completion_callback_(std::move(download_complete_callback)),
        drainer_(std::make_unique<mojo::DataPipeDrainer>(this,
                                                         std::move(consumer))) {
    if (!file_.IsValid()) {
      ClosePipeAndSignalMojoError();
      return;
    }
  }

  DownloadStreamReader(const DownloadStreamReader&) = delete;
  DownloadStreamReader& operator=(const DownloadStreamReader&) = delete;

  ~DownloadStreamReader() override = default;

  void OnMojoComplete(int32_t net_error, int64_t content_size) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    net_error_ = net_error;
    MaybeSignalComplete();
  }

  // mojo::DataPipeDrainer::Client overrides:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!file_.WriteAtCurrentPosAndCheck(data)) {
      ClosePipeAndSignalMojoError();
      return;
    }
    total_bytes_written_ += data.size();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(progress_callback_, total_bytes_written_));
  }

  void OnDataComplete() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    drainer_.reset();
    MaybeSignalComplete();
  }

 private:
  void ClosePipeAndSignalMojoError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    drainer_.reset();
    file_.Close();
    if (completion_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(completion_callback_),
                                    kErrorFailedToWriteFile, -1));
    }
  }

  // Invoked when either the Mojo call finishes or the data pipe closes. Both
  // need to happen before the download can be considered complete, but they may
  // occur in any order.
  void MaybeSignalComplete() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // The presence of `net_error_` indicates the Mojo call has completed. The
    // absence of `drainer_` indicates the pipe is closed. The presence of
    // `completion_callback_` indicates this isn't a duplicate call.
    if (net_error_.has_value() && !drainer_ && completion_callback_) {
      file_.Close();
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(completion_callback_),
                                    *net_error_, total_bytes_written_));
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::File file_;
  ProgressCallback progress_callback_;
  DownloadCompleteCallback completion_callback_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  int64_t total_bytes_written_ = 0;
  std::optional<int32_t> net_error_;
};

// Wraps a callback pair for PostRequest to log a network event.
std::pair<ResponseStartedCallback, PostRequestCompleteCallback>
WrapPostRequestCallbacksWithEventLogging(
    ResponseStartedCallback response_started_callback,
    PostRequestCompleteCallback post_request_complete_callback,
    const GURL& url,
    scoped_refptr<UpdaterEventLogger> event_logger) {
  if (!event_logger) {
    return std::make_pair(response_started_callback,
                          std::move(post_request_complete_callback));
  }

  scoped_refptr<base::RefCountedData<int>> response_code =
      base::MakeRefCounted<base::RefCountedData<int>>(0);
  return std::make_pair(
      base::BindRepeating(
          [](base::RefCountedData<int>* out_response_code,
             ResponseStartedCallback callback, int response_code,
             int64_t content_length) {
            out_response_code->data = response_code;
            callback.Run(response_code, content_length);
          },
          base::RetainedRef(response_code), response_started_callback),
      base::BindOnce(
          [](scoped_refptr<UpdaterEventLogger> event_logger,
             base::Time request_start_time,
             base::RefCountedData<int>* response_code, const GURL& url,
             PostRequestCompleteCallback callback,
             std::optional<std::string> response_body, int net_error,
             const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            proto::NetworkEvent event;
            event.set_stack(proto::NetworkEvent::DIRECT);
            event.set_url(url.spec());
            event.set_bytes_received(response_body ? response_body->size() : 0);
            event.set_elapsed_time_ms(
                (base::Time::Now() - request_start_time).InMilliseconds());
            if (net_error != 0) {
              event.set_error_code(net_error);
            } else if (response_code->data < 200 || response_code->data > 299) {
              event.set_error_code(response_code->data);
            }
            proto::Omaha4Metric metric;
            *metric.mutable_network_event() = std::move(event);
            event_logger->Log(std::move(metric));
            std::move(callback).Run(response_body, net_error, header_etag,
                                    header_x_cup_server_proof,
                                    header_set_cookie, xheader_retry_after_sec);
          },
          event_logger, base::Time::Now(), base::RetainedRef(response_code),
          url, std::move(post_request_complete_callback)));
}

std::pair<ResponseStartedCallback, DownloadCompleteCallback>
WrapDownloadToFileCallbacksWithEventLogging(
    ResponseStartedCallback response_started_callback,
    DownloadCompleteCallback download_complete_callback,
    const GURL& url,
    scoped_refptr<UpdaterEventLogger> event_logger) {
  if (!event_logger) {
    return std::make_pair(response_started_callback,
                          std::move(download_complete_callback));
  }

  scoped_refptr<base::RefCountedData<int>> response_code =
      base::MakeRefCounted<base::RefCountedData<int>>(0);
  return std::make_pair(
      base::BindRepeating(
          [](base::RefCountedData<int>* out_response_code,
             ResponseStartedCallback callback, int response_code,
             int64_t content_length) {
            out_response_code->data = response_code;
            callback.Run(response_code, content_length);
          },
          base::RetainedRef(response_code), response_started_callback),
      base::BindOnce(
          [](scoped_refptr<UpdaterEventLogger> event_logger,
             base::Time request_start_time,
             base::RefCountedData<int>* response_code, const GURL& url,
             DownloadCompleteCallback callback, int net_error,
             int64_t content_size) {
            proto::NetworkEvent event;
            event.set_stack(proto::NetworkEvent::DIRECT);
            event.set_url(url.spec());
            event.set_bytes_received(content_size);
            event.set_elapsed_time_ms(
                (base::Time::Now() - request_start_time).InMilliseconds());
            if (net_error != 0) {
              event.set_error_code(net_error);
            } else if (response_code->data < 200 || response_code->data > 299) {
              event.set_error_code(response_code->data);
            }
            proto::Omaha4Metric metric;
            *metric.mutable_network_event() = std::move(event);
            event_logger->Log(std::move(metric));
            std::move(callback).Run(net_error, content_size);
          },
          event_logger, base::Time::Now(), base::RetainedRef(response_code),
          url, std::move(download_complete_callback)));
}

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  explicit NetworkFetcher(scoped_refptr<UpdaterEventLogger> event_logger);
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;
  NetworkFetcher(const NetworkFetcher&) = delete;
  ~NetworkFetcher() override;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadCompleteCallback download_complete_callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdaterEventLogger> event_logger_;
};

NetworkFetcher::NetworkFetcher(scoped_refptr<UpdaterEventLogger> event_logger)
    : event_logger_(event_logger) {}

NetworkFetcher::~NetworkFetcher() = default;

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto [wrapped_response_started_callback,
        wrapped_post_request_complete_callback] =
      WrapPostRequestCallbacksWithEventLogging(
          response_started_callback, std::move(post_request_complete_callback),
          url, event_logger_);

  CRUUpdaterNetworkDataDelegate* delegate =
      [[CRUUpdaterNetworkDataDelegate alloc]
          initWithResponseStartedCallback:wrapped_response_started_callback
                         progressCallback:progress_callback
              postRequestCompleteCallback:
                  std::move(wrapped_post_request_complete_callback)];

  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration
                                                 .defaultSessionConfiguration
                                    delegate:delegate
                               delegateQueue:nil];

  NSMutableURLRequest* urlRequest =
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)];
  urlRequest.HTTPMethod = @"POST";
  urlRequest.HTTPBody = [[NSData alloc] initWithBytes:post_data.c_str()
                                               length:post_data.size()];
  [urlRequest setValue:base::SysUTF8ToNSString(GetUpdaterUserAgent())
      forHTTPHeaderField:@"User-Agent"];
  [urlRequest addValue:base::SysUTF8ToNSString(content_type)
      forHTTPHeaderField:@"Content-Type"];

  // Post additional headers could overwrite existing headers with the same key,
  // such as "Content-Type" above.
  for (const auto& [name, value] : post_additional_headers) {
    [urlRequest setValue:base::SysUTF8ToNSString(value)
        forHTTPHeaderField:base::SysUTF8ToNSString(name)];
  }
  VLOG(1) << "Posting data: " << post_data;

  NSURLSessionDataTask* dataTask = [session dataTaskWithRequest:urlRequest];
  [dataTask resume];
}

base::OnceClosure NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadCompleteCallback download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto [wrapped_response_started_callback, wrapped_download_complete_callback] =
      WrapDownloadToFileCallbacksWithEventLogging(
          response_started_callback, std::move(download_complete_callback), url,
          event_logger_);

  CRUUpdaterNetworkDownloadDelegate* delegate =
      [[CRUUpdaterNetworkDownloadDelegate alloc]
          initWithResponseStartedCallback:wrapped_response_started_callback
                         progressCallback:progress_callback
                                 filePath:file_path
                 downloadCompleteCallback:
                     std::move(wrapped_download_complete_callback)];

  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration
                                                 .defaultSessionConfiguration
                                    delegate:delegate
                               delegateQueue:nil];

  NSMutableURLRequest* urlRequest =
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)];
  [urlRequest setValue:base::SysUTF8ToNSString(GetUpdaterUserAgent())
      forHTTPHeaderField:@"User-Agent"];

  NSURLSessionDownloadTask* downloadTask =
      [session downloadTaskWithRequest:urlRequest];
  [downloadTask resume];
  // Cancellation is unimplemented.
  return base::DoNothing();
}

// The out-of-process fetcher creates a child worker process in the login
// context and delegates the network fetches to it. The idea is that the process
// the login context may have different access to the keychain or other
// resources for network transactions. This usually runs as a fallback solution
// after network failure in the startup context.
class OutOfProcessNetworkFetcher : public update_client::NetworkFetcher {
 public:
  explicit OutOfProcessNetworkFetcher(
      scoped_refptr<UpdaterEventLogger> event_logger);
  OutOfProcessNetworkFetcher& operator=(const OutOfProcessNetworkFetcher&) =
      delete;
  OutOfProcessNetworkFetcher(const NetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadCompleteCallback download_complete_callback) override;

 private:
  // Launches a Mojo net worker process and connects to it. Returns the
  // connection result.
  int DialFetchService();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdaterEventLogger> event_logger_;
  mojo::Remote<mojom::FetchService> remote_;
};

OutOfProcessNetworkFetcher::OutOfProcessNetworkFetcher(
    scoped_refptr<UpdaterEventLogger> event_logger)
    : event_logger_(event_logger) {}

int OutOfProcessNetworkFetcher::DialFetchService() {
  VLOG(2) << __func__;
  CHECK(!remote_.is_bound()) << "Fetcher cannot be reused.";

  // Gets the uid of the console user.
  std::optional<uid_t> user_id = [] -> std::optional<uid_t> {
    static constexpr char kConsoleFile[] = "/dev/console";
    struct stat stat = {};
    const int result = lstat(kConsoleFile, &stat);
    if (result != 0) {
      LOG(ERROR) << "Failed to stat " << kConsoleFile << ": " << result;
      return std::nullopt;
    }
    VLOG(2) << "Console user UID:" << stat.st_uid;
    return stat.st_uid;
  }();
  if (!user_id) {
    LOG(ERROR) << "No console user ID is found. The out of process fetcher "
               << "is not launched.";
    return kErrorNoConsoleUser;
  }

  // Gets updater binary path to run as the out-of-process fetcher.
  const base::FilePath updater_path = [] {
    base::FilePath updater_path;
    base::PathService::Get(base::FILE_EXE, &updater_path);
    return updater_path;
  }();

  // Creates a command line in the format of:
  //     /bin/launchctl asuser <uid> <updater> --net-worker \
  //          --mojo-platform-channel-handle=N
  // Note that base::CommandLine moves the switches ahead of arguments which
  // makes /bin/launchctl unhappy. Calls `PrependWrapper()` instead of
  // `AppendArg()` to make sure the arguments are in the required order.
  base::CommandLine launch_command(updater_path);
  launch_command.AppendSwitch(kNetWorkerSwitch);
  // Delegating to Mojo to "prepare" the command line appends the
  // `--mojo-platform-channel-handle=N` command line argument, so that the
  // network service knows which file descriptor name to recover, in order to
  // establish the primordial connection with this process.
  base::LaunchOptions options;
  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&options, &launch_command);
  launch_command.PrependWrapper(absl::StrFormat("%d", *user_id));
  launch_command.PrependWrapper("asuser");
  launch_command.PrependWrapper("/bin/launchctl");
  VLOG(2) << "Starting net-worker: " << launch_command.GetCommandLineString();
  base::Process child_process = base::LaunchProcess(launch_command, options);
  if (!child_process.IsValid()) {
    LOG(ERROR) << "Failed to launch out-of-process fetcher process.";
    return kErrorProcessLaunchFailed;
  }
  channel.RemoteProcessLaunchAttempted();
  mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
      channel.TakeLocalEndpoint(), {}, child_process.Handle());
  if (!pipe) {
    LOG(ERROR) << "Failed to send Mojo invitation to the fetcher process.";
    return kErrorMojoConnectionFailure;
  }
  mojo::PendingRemote<mojom::FetchService> pending_remote(
      std::move(pipe), mojom::FetchService::Version_);
  if (!pending_remote) {
    LOG(ERROR) << "Failed to establish IPC with the net-worker process.";
    return kErrorMojoConnectionFailure;
  }
  remote_ = mojo::Remote<mojom::FetchService>(std::move(pending_remote));
  return remote_.is_bound() ? kErrorOk : kErrorIpcDisconnect;
}

void OutOfProcessNetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto [wrapped_response_started_callback,
        wrapped_post_request_complete_callback] =
      WrapPostRequestCallbacksWithEventLogging(
          response_started_callback, std::move(post_request_complete_callback),
          url, event_logger_);

  VLOG(1) << __func__;
  if (const int dial_result = DialFetchService(); dial_result != kErrorOk) {
    LOG(ERROR) << "Failed to dial the fetch service: " << dial_result;
    std::move(post_request_complete_callback)
        .Run(std::nullopt, dial_result, {}, {}, {}, -1);
    return;
  }

  VLOG(2) << "OutOfProcessNetworkFetcher invoking PostRequest() on remote.";
  std::vector<mojom::HttpHeaderPtr> headers;
  for (const auto& [name, value] : post_additional_headers) {
    headers.push_back(mojom::HttpHeader::New(name, value));
  }

  remote_->PostRequest(
      url, post_data, content_type, std::move(headers),
      MakePostRequestObserver(
          response_started_callback, progress_callback,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(wrapped_post_request_complete_callback), std::nullopt,
              kErrorIpcDisconnect, "", "", "", -1)));
}

base::OnceClosure OutOfProcessNetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadCompleteCallback download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    std::move(download_complete_callback).Run(kErrorIpcDisconnect, -1);
    return base::DoNothing();
  }

  auto [wrapped_response_started_callback, wrapped_download_complete_callback] =
      WrapDownloadToFileCallbacksWithEventLogging(
          response_started_callback, std::move(download_complete_callback), url,
          event_logger_);

  auto reader = base::SequenceBound<DownloadStreamReader>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      std::move(consumer), file_path,
      base::BindPostTaskToCurrentDefault(progress_callback),
      base::BindPostTaskToCurrentDefault(
          std::move(wrapped_download_complete_callback)));

  if (const int dial_result = DialFetchService(); dial_result != kErrorOk) {
    LOG(ERROR) << "Failed to dial the fetch service: " << dial_result;
    std::move(download_complete_callback).Run(dial_result, -1);
    return base::DoNothing();
  }

  remote_->DownloadToStream(
      url, std::move(producer),
      MakeFileDownloadObserver(
          wrapped_response_started_callback,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(
                  [](base::SequenceBound<DownloadStreamReader> reader,
                     int32_t net_error, int64_t content_size) {
                    reader.AsyncCall(&DownloadStreamReader::OnMojoComplete)
                        .WithArgs(net_error, content_size);
                  },
                  std::move(reader)),
              kErrorIpcDisconnect, -1)));
  return base::DoNothing();
}

}  // namespace

NetworkStreamFetcher::NetworkStreamFetcher() = default;
NetworkStreamFetcher::~NetworkStreamFetcher() = default;

base::OnceClosure NetworkStreamFetcher::Download(
    const GURL& url,
    mojo::ScopedDataPipeProducerHandle response_stream,
    ResponseStartedCallback response_started_callback,
    DownloadCompleteCallback download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CRUUpdaterNetworkStreamDelegate* delegate =
      [[CRUUpdaterNetworkStreamDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                              destination:std::move(response_stream)
                 downloadCompleteCallback:std::move(
                                              download_complete_callback)];

  // Note that this NSURLSession leaks. Typically, `finishTasksAndInvalidate`
  // could be used to allow the session to complete its tasks before tearing
  // itself down. However, because streamed downloads can be suspended for flow
  // control, this mechanism would clean up tasks we intend to resume. It is
  // possible to resolve this leak by maintaining a reference to the session and
  // cleaning it up once all operations have completed. However, the out of
  // process fetcher lives about as long as the session anyways.
  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration
                                                 .defaultSessionConfiguration
                                    delegate:delegate
                               delegateQueue:nil];

  NSMutableURLRequest* urlRequest =
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)];
  [urlRequest setValue:base::SysUTF8ToNSString(GetUpdaterUserAgent())
      forHTTPHeaderField:@"User-Agent"];

  NSURLSessionDataTask* dataTask = [session dataTaskWithRequest:urlRequest];
  [dataTask resume];
  return base::DoNothing();
}

class NetworkFetcherFactory::Impl {
 public:
  explicit Impl(scoped_refptr<UpdaterEventLogger> event_logger)
      : event_logger_(event_logger) {}

  scoped_refptr<UpdaterEventLogger> event_logger() { return event_logger_; }

 private:
  scoped_refptr<UpdaterEventLogger> event_logger_;
};

NetworkFetcherFactory::NetworkFetcherFactory(
    std::optional<PolicyServiceProxyConfiguration>,
    scoped_refptr<UpdaterEventLogger> event_logger)
    : impl_(std::make_unique<Impl>(event_logger)) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<LoggingNetworkFetcher>(
      std::make_unique<FallbackNetFetcher>(
          std::make_unique<NetworkFetcher>(impl_->event_logger()),
          base::CommandLine::ForCurrentProcess()->HasSwitch(kNetWorkerSwitch)
              ? nullptr  // Already a networker, should not fallback further.
              : std::make_unique<OutOfProcessNetworkFetcher>(
                    impl_->event_logger())));
}

}  // namespace updater
