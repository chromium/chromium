// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_task_helper.h"

#import <Foundation/Foundation.h>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/public/background_service/download_params.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
bool g_ignore_localhost_ssl_error_for_testing = false;
}

using AuthenticationChallengeBlock =
    void (^)(NSURLSessionAuthChallengeDisposition disposition,
             NSURLCredential* credential);
using CompletionCallback =
    download::BackgroundDownloadTaskHelper::CompletionCallback;
using UpdateCallback = download::BackgroundDownloadTaskHelper::UpdateCallback;

@interface BackgroundDownloadDelegate : NSObject <NSURLSessionDownloadDelegate>
- (instancetype)initWithDownloadPath:(base::FilePath)downloadPath
                   completionHandler:(CompletionCallback)completionHandler
                       updateHandler:(UpdateCallback)updateHandler
                          taskRunner:
                              (scoped_refptr<base::SingleThreadTaskRunner>)
                                  taskRunner;

@end

@implementation BackgroundDownloadDelegate {
  base::FilePath _downloadPath;
  CompletionCallback _completionCallback;
  UpdateCallback _updateCallback;
  scoped_refptr<base::SingleThreadTaskRunner> _taskRunner;
}

- (instancetype)initWithDownloadPath:(base::FilePath)downloadPath
                   completionHandler:(CompletionCallback)completionHandler
                       updateHandler:(UpdateCallback)updateHandler
                          taskRunner:
                              (scoped_refptr<base::SingleThreadTaskRunner>)
                                  taskRunner {
  _downloadPath = downloadPath;
  _completionCallback = std::move(completionHandler);
  _updateCallback = updateHandler;
  _taskRunner = taskRunner;
  return self;
}

- (void)onDownloadCompletion:(bool)success
                     session:(NSURLSession*)session
                    filePath:(base::FilePath)filePath
                    fileSize:(int64_t)fileSize {
  if (_completionCallback) {
    // Invoke the completion callback on main thread.
    _taskRunner->PostTask(
        FROM_HERE, base::BindOnce(std::move(_completionCallback), success,
                                  filePath, fileSize));
  }
  [session invalidateAndCancel];
}

#pragma mark - NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession*)session
          downloadTask:(NSURLSessionDownloadTask*)downloadTask
     didResumeAtOffset:(int64_t)fileOffset
    expectedTotalBytes:(int64_t)expectedTotalBytes {
  DVLOG(1) << __func__ << " , offset:" << fileOffset
           << " , expectedTotalBytes:" << expectedTotalBytes;
  NOTIMPLEMENTED();
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
  DVLOG(1) << __func__ << ",byte written: " << bytesWritten
           << ", totalBytesWritten:" << totalBytesWritten
           << ", totalBytesExpectedToWrite:" << totalBytesExpectedToWrite;
  if (_updateCallback) {
    _taskRunner->PostTask(
        FROM_HERE, base::BindRepeating(_updateCallback, totalBytesWritten));
  }
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  DVLOG(1) << __func__;
  if (!location) {
    [self onDownloadCompletion:/*success=*/false
                       session:session
                      filePath:base::FilePath()
                      fileSize:0];
    return;
  }

  // Analyze the response code. Treat non http 200 as failure downloads.
  NSURLResponse* response = [downloadTask response];
  if (response && [response isKindOfClass:[NSHTTPURLResponse class]]) {
    NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
    if ([httpResponse statusCode] != 200) {
      [self onDownloadCompletion:/*success=*/false
                         session:session
                        filePath:base::FilePath()
                        fileSize:0];
      return;
    }
  }

  // Move the downloaded file from platform temporary directory to download
  // service's target directory. This must happen immediately on the current
  // thread or iOS may delete the file.
  const base::FilePath tempPath =
      base::mac::NSStringToFilePath([location path]);
  if (!base::Move(tempPath, _downloadPath)) {
    LOG(ERROR) << "Failed to move file from:" << tempPath
               << ", to:" << _downloadPath;
    [self onDownloadCompletion:/*success=*/false
                       session:session
                      filePath:base::FilePath()
                      fileSize:0];
    return;
  }

  // Get the file size on current thread.
  int64_t fileSize = 0;
  if (!base::GetFileSize(_downloadPath, &fileSize)) {
    LOG(ERROR) << "Failed to get file size from:" << _downloadPath;
    [self onDownloadCompletion:/*success=*/false
                       session:session
                      filePath:base::FilePath()
                      fileSize:0];
    return;
  }
  [self onDownloadCompletion:/*success=*/true
                     session:session
                    filePath:_downloadPath
                    fileSize:fileSize];
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  VLOG(1) << __func__;

  if (!error)
    return;

  [self onDownloadCompletion:/*success=*/false
                     session:session
                    filePath:base::FilePath()
                    fileSize:0];
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(AuthenticationChallengeBlock)completionHandler {
  DCHECK(completionHandler);
  if ([challenge.protectionSpace.authenticationMethod
          isEqualToString:NSURLAuthenticationMethodServerTrust]) {
    if (g_ignore_localhost_ssl_error_for_testing &&
        [challenge.protectionSpace.host isEqualToString:@"127.0.0.1"]) {
      NSURLCredential* credential = [NSURLCredential
          credentialForTrust:challenge.protectionSpace.serverTrust];
      completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
      return;
    }
  }
  completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}
@end

namespace download {

// Implementation of BackgroundDownloadTaskHelper based on
// NSURLSessionDownloadTask api.
// This class lives on main thread and all the callbacks will be invoked on main
// thread. The NSURLSessionDownloadDelegate it uses will broadcast download
// events on a background thread.
class BackgroundDownloadTaskHelperImpl : public BackgroundDownloadTaskHelper {
 public:
  BackgroundDownloadTaskHelperImpl() = default;
  ~BackgroundDownloadTaskHelperImpl() override = default;

 private:
  void StartDownload(const std::string& guid,
                     const base::FilePath& target_path,
                     const RequestParams& request_params,
                     const SchedulingParams& scheduling_params,
                     CompletionCallback completion_callback,
                     UpdateCallback update_callback) override {
    DCHECK(!guid.empty());
    DCHECK(!target_path.empty());
    // TODO(xingliu): Implement handleEventsForBackgroundURLSession and invoke
    // the callback passed from it.
    NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration
        backgroundSessionConfigurationWithIdentifier:
            base::SysUTF8ToNSString("background_download_" + guid)];
    configuration.sessionSendsLaunchEvents = YES;
    configuration.discretionary =
        scheduling_params.network_requirements !=
            SchedulingParams::NetworkRequirements::NONE ||
        scheduling_params.battery_requirements !=
            SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
    BackgroundDownloadDelegate* delegate = [[BackgroundDownloadDelegate alloc]
        initWithDownloadPath:target_path
           completionHandler:std::move(completion_callback)
               updateHandler:update_callback
                  taskRunner:base::ThreadTaskRunnerHandle::Get()];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration
                                                          delegate:delegate
                                                     delegateQueue:nil];
    NSURL* url = net::NSURLWithGURL(request_params.url);
    NSMutableURLRequest* request =
        [[NSMutableURLRequest alloc] initWithURL:url];
    [request setHTTPMethod:base::SysUTF8ToNSString(request_params.method)];
    net::HttpRequestHeaders::Iterator it(request_params.request_headers);
    while (it.GetNext()) {
      [request setValue:base::SysUTF8ToNSString(it.value())
          forHTTPHeaderField:base::SysUTF8ToNSString(it.name())];
    }

    NSURLSessionDownloadTask* downloadTask =
        [session downloadTaskWithRequest:request];
    [downloadTask resume];
  }
};

// static
std::unique_ptr<BackgroundDownloadTaskHelper>
BackgroundDownloadTaskHelper::Create() {
  return std::make_unique<BackgroundDownloadTaskHelperImpl>();
}

// static
void BackgroundDownloadTaskHelper::SetIgnoreLocalSSLErrorForTesting(
    bool ignore) {
  g_ignore_localhost_ssl_error_for_testing = ignore;
}

}  // namespace download
