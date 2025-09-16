// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_features.h"

#include "build/build_config.h"

namespace download {
namespace features {

BASE_FEATURE(kParallelDownloading,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
BASE_FEATURE(kBackoffInDownloading, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool IsBackoffInDownloadingEnabled() {
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
  return false;
#else
  return base::FeatureList::IsEnabled(kBackoffInDownloading);
#endif
}

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSmartSuggestionForLargeDownloads,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefreshExpirationDate, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDisplayInitiatorOrigin,
             "DownloadsDisplayInitiatorOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadNotificationServiceUnifiedAPI,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUseInProgressDownloadManagerForDownloadService,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDownloadResumptionWithoutStrongValidators,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUseParallelRequestsForHTTP2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseParallelRequestsForQUIC, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeleteExpiredDownloads, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeleteOverwrittenDownloads, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowFileBufferSizeControl, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowedMixedContentInlinePdf, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAsyncNotificationManagerForDownload,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableSavePackageForOffTheRecord,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace features

}  // namespace download
