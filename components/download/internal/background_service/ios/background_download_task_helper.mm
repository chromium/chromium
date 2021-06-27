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
#include "components/download/public/background_service/download_params.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using CompletionCallback =
    download::BackgroundDownloadTaskHelper::CompletionCallback;

@interface BackgroundDownloadDelegate : NSObject <NSURLSessionDownloadDelegate>
- (instancetype)initWithDownloadDirectory:(base::FilePath)downloadDir
                        completionHandler:(CompletionCallback)completionHandler;
@end

@implementation BackgroundDownloadDelegate {
  base::FilePath _downloadDir;
  CompletionCallback _completionCallback;
}

- (instancetype)initWithDownloadDirectory:(base::FilePath)downloadDir
                        completionHandler:
                            (CompletionCallback)completionHandler {
  _downloadDir = downloadDir;
  _completionCallback = completionHandler;
  return self;
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
  NOTIMPLEMENTED();
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  DVLOG(1) << __func__;
  if (!location) {
    _completionCallback.Run(/*success=*/false, base::FilePath());
    return;
  }

  // Make sure the target directory exists.
  if (!base::CreateDirectory(_downloadDir)) {
    LOG(ERROR) << "Failed to create dir:" << _downloadDir;
    _completionCallback.Run(/*success=*/false, base::FilePath());
    return;
  }

  // Move the downloaded file from platform temporary directory to download
  // service's target directory.
  const base::FilePath tempPath =
      base::mac::NSStringToFilePath([location path]);
  // TODO(xingliu): Rename the file to use the guid of the download.
  base::FilePath newFile = _downloadDir.Append(base::NumberToString(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds()));
  if (!base::Move(tempPath, newFile)) {
    LOG(ERROR) << "Failed to move file from:" << tempPath
               << ", to:" << _downloadDir;
    _completionCallback.Run(/*success=*/false, base::FilePath());
    return;
  }
  _completionCallback.Run(/*success=*/true, newFile);
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  VLOG(1) << __func__;
  // TODO(xingliu): Check whether we can resume for a few times if the user
  // terminated the app in multitask window or failed downloads.
  NOTIMPLEMENTED();
}
@end

namespace download {

// Implementation of BackgroundDownloadTaskHelper.
class BackgroundDownloadTaskHelperImpl : public BackgroundDownloadTaskHelper {
 public:
  BackgroundDownloadTaskHelperImpl(const base::FilePath& download_dir)
      : download_dir_(download_dir) {}
  ~BackgroundDownloadTaskHelperImpl() override = default;

 private:
  void StartDownload(const DownloadParams& download_params,
                     CompletionCallback completion_callback) override {
    // TODO(xingliu): Support more parameters in download::DownloadParams, the
    // session id should have guid as suffix. Implement
    // handleEventsForBackgroundURLSession and invoke the callback passed from
    // it.
    NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration
        backgroundSessionConfigurationWithIdentifier:@"background_download"];
    configuration.sessionSendsLaunchEvents = YES;
    configuration.discretionary = NO;
    BackgroundDownloadDelegate* delegate = [[BackgroundDownloadDelegate alloc]
        initWithDownloadDirectory:download_dir_
                completionHandler:completion_callback];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration
                                                          delegate:delegate
                                                     delegateQueue:nil];
    NSURL* url = net::NSURLWithGURL(download_params.request_params.url);

    NSURLRequest* request = [NSURLRequest requestWithURL:url];
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
