// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network.h"

#import <Foundation/Foundation.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"
#import "net/base/mac/url_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  base::scoped_nsobject<NSMutableData> _downloadedData;
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
    _downloadedData.reset([[NSMutableData alloc] init]);
  }
  return self;
}

#pragma mark - NSURLSessionDataDelegate

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  [_downloadedData appendData:data];

  int64_t current = 0;

  if (dataTask.countOfBytesExpectedToReceive > 0) {
    current = (dataTask.countOfBytesReceived * 100) /
              dataTask.countOfBytesExpectedToReceive;
  } else {
    current = 100;
  }
  _callbackRunner->PostTask(FROM_HERE,
                            base::BindOnce(_progressCallback, current));
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
  completionHandler(NULL);
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  if (!location)
    return;

  const base::FilePath tempPath =
      base::mac::NSStringToFilePath([location path]);
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

namespace base {
class SequencedTaskRunner;
}

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

  void DownloadToFile(
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

  base::scoped_nsobject<CRUUpdaterNetworkDataDelegate> delegate(
      [[CRUUpdaterNetworkDataDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
              postRequestCompleteCallback:std::move(
                                              post_request_complete_callback)]);

  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration
                                                 defaultSessionConfiguration]
                                    delegate:delegate
                               delegateQueue:nil];

  base::scoped_nsobject<NSMutableURLRequest> urlRequest(
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)]);
  [urlRequest setHTTPMethod:@"POST"];
  base::scoped_nsobject<NSData> body(
      [[NSData alloc] initWithBytes:post_data.c_str() length:post_data.size()]);
  [urlRequest setHTTPBody:body];
  [urlRequest addValue:base::SysUTF8ToNSString(content_type)
      forHTTPHeaderField:@"Content-Type"];

  // Post additional headers could overwrite existing headers with the same key,
  // such as "Content-Type" above.
  for (const auto& header : post_additional_headers) {
    [urlRequest setValue:base::SysUTF8ToNSString(header.second)
        forHTTPHeaderField:base::SysUTF8ToNSString(header.first)];
  }
  VLOG(1) << "Posting data: " << post_data.c_str();

  NSURLSessionDataTask* dataTask = [session dataTaskWithRequest:urlRequest];
  [dataTask resume];
}

void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::scoped_nsobject<CRUUpdaterNetworkDownloadDelegate> delegate(
      [[CRUUpdaterNetworkDownloadDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
                                 filePath:file_path
           downloadToFileCompleteCallback:
               std::move(download_to_file_complete_callback)]);

  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration
                                                 defaultSessionConfiguration]
                                    delegate:delegate
                               delegateQueue:nil];

  base::scoped_nsobject<NSMutableURLRequest> urlRequest(
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)]);

  NSURLSessionDownloadTask* downloadTask =
      [session downloadTaskWithRequest:urlRequest];
  [downloadTask resume];
}

}  // namespace

class NetworkFetcherFactory::Impl {};

NetworkFetcherFactory::NetworkFetcherFactory(
    absl::optional<PolicyServiceProxyConfiguration>) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<NetworkFetcher>();
}

}  // namespace updater
