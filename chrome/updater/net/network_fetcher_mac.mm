// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#import "base/apple/foundation_util.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/net/fallback_net_fetcher.h"
#include "chrome/updater/net/fetcher_callback_adapter.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/net/network_file_fetcher.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#import "net/base/apple/url_conversions.h"
#include "url/gurl.h"

using ResponseStartedCallback =
    update_client::NetworkFetcher::ResponseStartedCallback;
using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
using PostRequestCompleteCallback =
    update_client::NetworkFetcher::PostRequestCompleteCallback;
using DownloadToFileCompleteCallback =
    update_client::NetworkFetcher::DownloadToFileCompleteCallback;

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
  int64_t retryAfterResult = -1;
  NSString* xRetryAfter = [headers
      objectForKey:base::SysUTF8ToNSString(
                       update_client::NetworkFetcher::kHeaderXRetryAfter)];
  if (xRetryAfter) {
    retryAfterResult = [xRetryAfter intValue];
  }

  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(_postRequestCompleteCallback),
                     std::make_unique<std::string>(
                         reinterpret_cast<const char*>([_downloadedData bytes]),
                         [_downloadedData length]),
                     error.code, base::SysNSStringToUTF8(etag),
                     base::SysNSStringToUTF8(cupServerProof),
                     retryAfterResult));
}

@end

@interface CRUUpdaterNetworkDownloadDelegate
    : CRUUpdaterNetworkController <NSURLSessionDownloadDelegate>
- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
                           filePath:(const base::FilePath&)filePath
     downloadToFileCompleteCallback:
         (DownloadToFileCompleteCallback)downloadToFileCompleteCallback;
@end

@implementation CRUUpdaterNetworkDownloadDelegate {
  base::FilePath _filePath;
  bool _moveTempFileSuccessful;
  DownloadToFileCompleteCallback _downloadToFileCompleteCallback;
}

- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
                           filePath:(const base::FilePath&)filePath
     downloadToFileCompleteCallback:
         (DownloadToFileCompleteCallback)downloadToFileCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:progressCallback]) {
    _filePath = filePath;
    _moveTempFileSuccessful = false;
    _downloadToFileCompleteCallback = std::move(downloadToFileCompleteCallback);
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
      FROM_HERE, base::BindOnce(std::move(_downloadToFileCompleteCallback),
                                result, [task countOfBytesReceived]));
}

@end

@interface CRUUpdaterNetworkDownloadDataDelegate
    : CRUUpdaterNetworkController <NSURLSessionDataDelegate>
- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
                             output:(base::File)output
     downloadToFileCompleteCallback:
         (DownloadToFileCompleteCallback)downloadToFileCompleteCallback;
@end

@implementation CRUUpdaterNetworkDownloadDataDelegate {
  base::File _output;
  DownloadToFileCompleteCallback _downloadToFileCompleteCallback;
}

- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
                             output:(base::File)output
     downloadToFileCompleteCallback:
         (DownloadToFileCompleteCallback)downloadToFileCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:progressCallback]) {
    _output = std::move(output);
    _downloadToFileCompleteCallback = std::move(downloadToFileCompleteCallback);
  }
  return self;
}

#pragma mark - NSURLSessionDataDelegate

// Write the downloaded contents to the file. Cancels the download if there's
// write error. It's up to the caller to handle the partially written file.
- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  if (_output.WriteAtCurrentPosAndCheck(base::apple::NSDataToSpan(data))) {
    _callbackRunner->PostTask(
        FROM_HERE, base::BindOnce(_progressCallback, _output.GetLength()));
    [dataTask resume];
  } else {
    VLOG(1) << __func__ << ": File write error, download job cancelled.";
    if (_downloadToFileCompleteCallback) {
      _callbackRunner->PostTask(
          FROM_HERE, base::BindOnce(std::move(_downloadToFileCompleteCallback),
                                    updater::kErrorFailedToWriteFile, -1));
    }
    [dataTask cancel];
  }
}

// Tells the delegate that the data task received the initial reply from the
// server.
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

  NSInteger result;
  if (error) {
    result = error.code;
    DLOG(ERROR) << "NSError code: " << result
                << ". NSErrorDomain: " << base::SysNSStringToUTF8(error.domain)
                << ". NSError description: "
                << base::SysNSStringToUTF8(error.description);
  } else {
    NSHTTPURLResponse* response = (NSHTTPURLResponse*)task.response;
    result = response.statusCode == 200 ? 0 : response.statusCode;
  }
  if (_downloadToFileCompleteCallback) {
    _callbackRunner->PostTask(
        FROM_HERE, base::BindOnce(std::move(_downloadToFileCompleteCallback),
                                  result, [task countOfBytesReceived]));
  }
}

@end

namespace updater {
namespace {

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  NetworkFetcher();
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;
  NetworkFetcher(const NetworkFetcher&) = delete;
  ~NetworkFetcher() override;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

NetworkFetcher::NetworkFetcher() = default;

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

  CRUUpdaterNetworkDataDelegate* delegate =
      [[CRUUpdaterNetworkDataDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
              postRequestCompleteCallback:std::move(
                                              post_request_complete_callback)];

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
  VLOG(1) << "Posting data: " << post_data.c_str();

  NSURLSessionDataTask* dataTask = [session dataTaskWithRequest:urlRequest];
  [dataTask resume];
}

base::OnceClosure NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CRUUpdaterNetworkDownloadDelegate* delegate =
      [[CRUUpdaterNetworkDownloadDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
                                 filePath:file_path
           downloadToFileCompleteCallback:
               std::move(download_to_file_complete_callback)];

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
  return base::DoNothing();
}

// The out-of-process fetcher creates a child worker process in the login
// context and delegates the network fetches to it. The idea is that the process
// the login context may have different access to the keychain or other
// resources for network transactions. This usually runs as a fallback solution
// after network failure in the startup context.
class OutOfProcessNetworkFetcher : public update_client::NetworkFetcher {
 public:
  OutOfProcessNetworkFetcher() = default;
  OutOfProcessNetworkFetcher& operator=(const OutOfProcessNetworkFetcher&) =
      delete;
  OutOfProcessNetworkFetcher(const NetworkFetcher&) = delete;
  ~OutOfProcessNetworkFetcher() override = default;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback) override;

 private:
  // Launches a Mojo net worker process and connects to it. Returns the
  // connection result.
  int DialFetchService();

  void DoDownloadFile(
      const GURL& url,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_complete_callback,
      base::File output);

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<mojom::FetchService> remote_;
};

int OutOfProcessNetworkFetcher::DialFetchService() {
  VLOG(2) << __func__;
  CHECK(!remote_.is_bound()) << "Fetcher cannot be reused.";

  // Gets the uid of the console user.
  std::optional<uid_t> user_id = []() -> std::optional<uid_t> {
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
  launch_command.PrependWrapper(base::StringPrintf("%d", *user_id));
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
  VLOG(1) << __func__;
  if (const int dial_result = DialFetchService(); dial_result != kErrorOk) {
    LOG(ERROR) << "Failed to dial the fetch service: " << dial_result;
    std::move(post_request_complete_callback)
        .Run(nullptr, dial_result, {}, {}, -1);
    return;
  }

  VLOG(2) << "OutOfProcessNetworkFetcher invoking PostRequest() on remote.";
  std::vector<mojom::HttpHeaderPtr> headers;
  for (const auto& [name, value] : post_additional_headers) {
    headers.push_back(mojom::HttpHeader::New(name, value));
  }
  remote_->PostRequest(
      url, post_data, content_type, std::move(headers),
      MakePostRequestObserver(response_started_callback, progress_callback,
                              std::move(post_request_complete_callback)));
}

base::OnceClosure OutOfProcessNetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& file_path) {
            return base::File(file_path, base::File::FLAG_OPEN_ALWAYS |
                                             base::File::FLAG_WRITE);
          },
          file_path),
      base::BindOnce(&OutOfProcessNetworkFetcher::DoDownloadFile,
                     base::Unretained(this), url, response_started_callback,
                     progress_callback, std::move(download_complete_callback)));
  return base::DoNothing();
}

void OutOfProcessNetworkFetcher::DoDownloadFile(
    const GURL& url,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_complete_callback,
    base::File output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  if (!output.IsValid()) {
    LOG(ERROR) << "Failed to open the file to download.";
    std::move(download_complete_callback).Run(kErrorFailedToWriteFile, -1);
    return;
  }

  if (const int dial_result = DialFetchService(); dial_result != kErrorOk) {
    LOG(ERROR) << "Failed to dial the fetch service: " << dial_result;
    std::move(download_complete_callback).Run(dial_result, -1);
    return;
  }
  VLOG(2) << "OutOfProcessNetworkFetcher invoking DownloadToFile() on remote.";
  remote_->DownloadToFile(
      url, std::move(output),
      MakeFileDownloadObserver(response_started_callback, progress_callback,
                               std::move(download_complete_callback)));
}

}  // namespace

base::OnceClosure NetworkFileFetcher::Download(
    const GURL& url,
    base::File output,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CRUUpdaterNetworkDownloadDataDelegate* delegate =
      [[CRUUpdaterNetworkDownloadDataDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
                                   output:std::move(output)
           downloadToFileCompleteCallback:
               std::move(download_to_file_complete_callback)];

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

class NetworkFetcherFactory::Impl {};

NetworkFetcherFactory::NetworkFetcherFactory(
    std::optional<PolicyServiceProxyConfiguration>) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<FallbackNetFetcher>(
      std::make_unique<NetworkFetcher>(),
      base::CommandLine::ForCurrentProcess()->HasSwitch(kNetWorkerSwitch)
          ? nullptr  // Already a networker, should not fallback further.
          : std::make_unique<OutOfProcessNetworkFetcher>());
}

}  // namespace updater
