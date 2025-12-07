// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/features.h"

#include "build/build_config.h"
#include "components/history/core/browser/features.h"

namespace browsing_data::features {
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBrowsingDataModel, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kDbdRevampDesktop, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPasswordRemovalExtensionErrorKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBrowsingHistoryActorIntegrationM1,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsBrowsingHistoryActorIntegrationM1Enabled() {
  return base::FeatureList::IsEnabled(kBrowsingHistoryActorIntegrationM1) ||
         base::FeatureList::IsEnabled(
             history::kBrowsingHistoryActorIntegrationM2);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}  // namespace browsing_data::features
