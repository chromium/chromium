// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_features.h"

#include "build/build_config.h"

namespace download {
namespace features {

const base::Feature kUseDownloadOfflineContentProvider{
    "UseDownloadOfflineContentProvider", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadAutoResumptionNative {
  "DownloadsAutoResumptionNative",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kParallelDownloading {
  "ParallelDownloading",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if defined(OS_ANDROID)
const base::Feature kRefreshExpirationDate{"RefreshExpirationDate",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif

const base::Feature kUseInProgressDownloadManagerForDownloadService{
    "UseInProgressDownloadManagerForDownloadService",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAllowDownloadResumptionWithoutStrongValidators{
  "AllowDownloadResumptionWithoutStrongValidators",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kUseParallelRequestsForUnknwonRangeSupport{
    "UseParallelRequestForUnknownRangeSupport",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseParallelRequestsForHTTP2{
    "UseParallelRequestsForHTTP2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseParallelRequestsForQUIC{
    "UseParallelRequestsForQUIC", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDeleteExpiredDownloads{"DeleteExpiredDownloads",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features

}  // namespace download
