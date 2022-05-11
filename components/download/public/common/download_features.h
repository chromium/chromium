// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/download/public/common/download_export.h"

namespace download {
namespace features {

// The Finch parameter to control whether download later dialog should show the
// date time picker option.
constexpr char kDownloadLaterShowDateTimePicker[] = "show_date_time_picker";

// The Finch parameter to control the minimum download file size to show the
// download later dialog.
constexpr char kDownloadLaterMinFileSizeKb[] = "min_file_size_kb";

// Whether offline content provider should be used for the downloads UI..
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kUseDownloadOfflineContentProvider;

// Whether download auto-resumptions are enabled in native.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kDownloadAutoResumptionNative;

// Whether a download can be handled by parallel jobs.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature kParallelDownloading;

// Whether to enable download later feature.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature kDownloadLater;

#if BUILDFLAG(IS_ANDROID)
// Whether download expiration date will be refreshed on resumption.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature kRefreshExpirationDate;

// Whether to enable smart suggestion for large downloads
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kSmartSuggestionForLargeDownloads;
#endif

// Whether in-progress download manager will be used to initialize download
// service.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kUseInProgressDownloadManagerForDownloadService;

// Whether download resumption is allowed when there are no strong validators.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kAllowDownloadResumptionWithoutStrongValidators;

// Whether parallel download is used for HTTP2 connections.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kUseParallelRequestsForHTTP2;

// Whether parallel download is used for QUIC connections.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kUseParallelRequestsForQUIC;

// Whether to delete expired download.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature kDeleteExpiredDownloads;

// Whether to delete downloads that are overwritten by others.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kDeleteOverwrittenDownloads;

// Whether to allow changing the size of file buffer.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kAllowFileBufferSizeControl;

// Arbitrary range request support for download system.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature kDownloadRange;
}  // namespace features

namespace switches {

// If set, show the download later dialog without the requirement of being on
// cellular network.
COMPONENTS_DOWNLOAD_EXPORT extern const char kDownloadLaterDebugOnWifi[];

}  // namespace switches

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_
