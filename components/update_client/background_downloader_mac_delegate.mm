// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/update_client/background_downloader_mac_delegate.h"

#import <Foundation/Foundation.h>

#include <cstdint>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/update_client/update_client_errors.h"
#include "url/gurl.h"

@implementation UpdateClientDownloadDelegate {
  base::FilePath _download_cache;
  UpdateClientDelegateDownloadCompleteCallback _download_complete_callback;
  UpdateClientDelegateMetricsCollectedCallback _metrics_collected_callback;
  UpdateClientDelegateDownloadProgressCallback _progress_callback;
  scoped_refptr<base::SequencedTaskRunner> _callback_runner;
}

- (instancetype)
       initWithDownloadCache:(base::FilePath)downloadCache
    downloadCompleteCallback:
        (UpdateClientDelegateDownloadCompleteCallback)downloadCompleteCallback
    metricsCollectedCallback:
        (UpdateClientDelegateMetricsCollectedCallback)metricsCollectedCallback
            progressCallback:
                (UpdateClientDelegateDownloadProgressCallback)progressCallback {
  if (self = [super init]) {
    _download_cache = downloadCache;
    _download_complete_callback = downloadCompleteCallback;
    _metrics_collected_callback = metricsCollectedCallback;
    _progress_callback = progressCallback;
    _callback_runner = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return self;
}

- (void)endTask:(NSURLSessionTask*)task
    withLocation:(std::optional<base::FilePath>)location
       withError:(int)error {
  _callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(_download_complete_callback,
                     update_client::GURLWithNSURL(task.originalRequest.URL),
                     location.value_or(base::FilePath()), error,
                     task.countOfBytesReceived,
                     task.countOfBytesExpectedToReceive));
}

#pragma mark - NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  if (!base::PathExists(_download_cache) &&
      !base::CreateDirectory(_download_cache)) {
    VLOG(1) << "Failed to create download cache directory at: "
            << _download_cache;
    [self endTask:downloadTask
        withLocation:std::nullopt
           withError:static_cast<int>(update_client::CrxDownloaderError::
                                          MAC_BG_CANNOT_CREATE_DOWNLOAD_CACHE)];
    return;
  }

  const base::FilePath temp_path =
      base::apple::NSStringToFilePath(location.path);
  base::FilePath cache_path =
      _download_cache.Append(update_client::URLToFilename(
          update_client::GURLWithNSURL(downloadTask.originalRequest.URL)));
  if (!base::Move(temp_path, cache_path)) {
    DVLOG(1)
        << "Failed to move the downloaded file from the temporary location: "
        << temp_path << " to: " << cache_path;
    [self endTask:downloadTask
        withLocation:std::nullopt
           withError:static_cast<int>(update_client::CrxDownloaderError::
                                          MAC_BG_MOVE_TO_CACHE_FAIL)];
    return;
  }

  [self endTask:downloadTask
      withLocation:cache_path
         withError:static_cast<int>(update_client::CrxDownloaderError::NONE)];
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(nonnull NSURLSessionDownloadTask*)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
  if (bytesWritten > 0) {
    _callback_runner->PostTask(
        FROM_HERE, base::BindOnce(_progress_callback,
                                  update_client::GURLWithNSURL(
                                      downloadTask.originalRequest.URL)));
  }
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(nonnull NSURLSessionTask*)task
    didCompleteWithError:(nullable NSError*)error {
  if (error) {
    [self endTask:task withLocation:std::nullopt withError:error.code];
  }
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)metrics {
  _callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(_metrics_collected_callback,
                     update_client::GURLWithNSURL(task.originalRequest.URL),
                     metrics.taskInterval.duration *
                         base::TimeTicks::kMillisecondsPerSecond));
}

@end

namespace update_client {

GURL GURLWithNSURL(NSURL* url) {
  return url ? GURL(url.absoluteString.UTF8String) : GURL();
}

base::FilePath URLToFilename(const GURL& url) {
  uint32_t hash = base::PersistentHash(url.spec());
  return base::FilePath::FromASCII(
      base::HexEncode(reinterpret_cast<uint8_t*>(&hash), sizeof(hash)));
}

}  // namespace update_client
