// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_features.h"

#include "build/build_config.h"

namespace download {
namespace features {

BASE_FEATURE(kParallelDownloading,
             "ParallelDownloading",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSmartSuggestionForLargeDownloads,
             "SmartSuggestionForLargeDownloads",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefreshExpirationDate,
             "RefreshExpirationDate",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDownloadsMigrateToJobsAPI,
             "DownloadsMigrateToJobsAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadNotificationServiceUnifiedAPI,
             "DownloadNotificationServiceUnifiedAPI",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUseInProgressDownloadManagerForDownloadService,
             "UseInProgressDownloadManagerForDownloadService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDownloadResumptionWithoutStrongValidators,
             "AllowDownloadResumptionWithoutStrongValidators",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUseParallelRequestsForHTTP2,
             "UseParallelRequestsForHTTP2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseParallelRequestsForQUIC,
             "UseParallelRequestsForQUIC",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeleteExpiredDownloads,
             "DeleteExpiredDownloads",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeleteOverwrittenDownloads,
             "DeleteOverwrittenDownloads",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowFileBufferSizeControl,
             "AllowFileBufferSizeControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowedMixedContentInlinePdf,
             "AllowedMixedContentInlinePdf",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

}  // namespace download
