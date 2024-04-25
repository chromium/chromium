// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace update_client::features {
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kDynamicCrxDownloaderPriority,
             "DynamicCrxDownloaderPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kDynamicCrxDownloaderPrioritySizeThreshold{
    &kDynamicCrxDownloaderPriority, "BackgroundCrxDownloaderSizeThreshold",
    10000000 /*10 MB*/};
#endif
}  // namespace update_client::features
