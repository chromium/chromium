// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_MAC_DELEGATE_H_
#define COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_MAC_DELEGATE_H_

#import <Foundation/Foundation.h>

#include <cstdint>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

class GURL;

// Callback invoked by DownloadDelegate when a download has finished.
using UpdateClientDelegateDownloadCompleteCallback = base::RepeatingCallback<
    void(const GURL&, const base::FilePath&, int, int64_t, int64_t)>;

// Callback invoked by DownloadDelegate when download metrics are available.
using UpdateClientDelegateMetricsCollectedCallback =
    base::RepeatingCallback<void(const GURL& url, uint64_t download_time_ms)>;

// Callback invoked by DownloadDelegate when progress has been made on a task.
using UpdateClientDelegateDownloadProgressCallback =
    base::RepeatingCallback<void(const GURL&)>;

COMPONENT_EXPORT(BACKGROUND_DOWNLOADER_DELEGATE)
@interface UpdateClientDownloadDelegate
    : NSObject <NSURLSessionDownloadDelegate>
- (instancetype)
       initWithDownloadCache:(base::FilePath)downloadCache
    downloadCompleteCallback:
        (UpdateClientDelegateDownloadCompleteCallback)downloadCompleteCallback
    metricsCollectedCallback:
        (UpdateClientDelegateMetricsCollectedCallback)metricsCollectedCallback
            progressCallback:
                (UpdateClientDelegateDownloadProgressCallback)progressCallback;
@end

namespace update_client {

COMPONENT_EXPORT(BACKGROUND_DOWNLOADER_DELEGATE)
GURL GURLWithNSURL(NSURL* url);

COMPONENT_EXPORT(BACKGROUND_DOWNLOADER_DELEGATE)
base::FilePath URLToFilename(const GURL& url);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_MAC_DELEGATE_H_
