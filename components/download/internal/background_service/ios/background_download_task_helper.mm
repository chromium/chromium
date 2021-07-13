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
#include "components/download/public/background_service/download_params.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using CompletionCallback =
    download::BackgroundDownloadTaskHelper::CompletionCallback;
using UpdateCallback = download::BackgroundDownloadTaskHelper::UpdateCallback;

@interface BackgroundDownloadDelegate : NSObject <NSURLSessionDownloadDelegate>
- (instancetype)initWithDownloadDirectory:(base::FilePath)downloadDir
                                     guid:(std::string)guid
                        completionHandler:(CompletionCallback)completionHandler
                            updateHandler:(UpdateCallback)updateHandler;
@end

@implementation BackgroundDownloadDelegate {
  base::FilePath _downloadDir;
  std::string _guid;
  CompletionCallback _completionCallback;
  UpdateCallback _updateCallback;
}

- (instancetype)initWithDownloadDirectory:(base::FilePath)downloadDir
                                     guid:(std::string)guid
                        completionHandler:(CompletionCallback)completionHandler
                            updateHandler:(UpdateCallback)updateHandler {
  _downloadDir = downloadDir;
  _guid = guid;
  _completionCallback = std::move(completionHandler);
  _updateCallback = updateHandler;
  return self;
}

- (void)invokeCompletionHandler:(bool)success
                       filePath:(base::FilePath)filePath {
  if (_completionCallback)
    std::move(_completionCallback).Run(success, filePath);
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
  if (_updateCallback)
    _updateCallback.Run(totalBytesWritten);
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  DVLOG(1) << __func__;
  if (!location) {
    [self invokeCompletionHandler:/*success=*/false filePath:base::FilePath()];
    return;
  }

  // Make sure the target directory exists.
  if (!base::CreateDirectory(_downloadDir)) {
    LOG(ERROR) << "Failed to create dir:" << _downloadDir;
    [self invokeCompletionHandler:/*success=*/false filePath:base::FilePath()];
    return;
  }

  // Move the downloaded file from platform temporary directory to download
  // service's target directory.
  const base::FilePath tempPath =
      base::mac::NSStringToFilePath([location path]);
  base::FilePath newFile = _downloadDir.AppendASCII(_guid);
  if (!base::Move(tempPath, newFile)) {
    LOG(ERROR) << "Failed to move file from:" << tempPath
               << ", to:" << _downloadDir;
    [self invokeCompletionHandler:/*success=*/false filePath:base::FilePath()];
    return;
  }
  [self invokeCompletionHandler:/*success=*/true filePath:newFile];
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  VLOG(1) << __func__;
  // TODO(xingliu): Check whether we can resume for a few times if the user
  // terminated the app in multitask window or failed downloads.
}
@end

namespace download {

// Implementation of BackgroundDownloadTaskHelper based on
// NSURLSessionDownloadTask api.
class BackgroundDownloadTaskHelperImpl : public BackgroundDownloadTaskHelper {
 public:
  BackgroundDownloadTaskHelperImpl(const base::FilePath& download_dir)
      : download_dir_(download_dir) {}
  ~BackgroundDownloadTaskHelperImpl() override = default;

 private:
  void StartDownload(const std::string& guid,
                     const RequestParams& request_params,
                     const SchedulingParams& scheduling_params,
                     CompletionCallback completion_callback,
                     UpdateCallback update_callback) override {
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
        initWithDownloadDirectory:download_dir_
                             guid:guid
                completionHandler:std::move(completion_callback)
                    updateHandler:update_callback];
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

  // A directory to hold download service files. The files in here will be
  // pruned frequently.
  const base::FilePath download_dir_;
};

// static
std::unique_ptr<BackgroundDownloadTaskHelper>
BackgroundDownloadTaskHelper::Create(const base::FilePath& download_dir) {
  return std::make_unique<BackgroundDownloadTaskHelperImpl>(download_dir);
}

}  // namespace download
