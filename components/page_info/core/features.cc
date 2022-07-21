// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

namespace page_info {

#if BUILDFLAG(IS_ANDROID)
const base::Feature kPageInfoHistory{"PageInfoHistory",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kPageInfoStoreInfo{"PageInfoStoreInfo",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPageInfoDiscoverability{"PageInfoDiscoverability",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif

extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale) {
  if (l10n_util::GetLanguage(locale) == "en") {
    return base::FeatureList::IsEnabled(kPageInfoAboutThisSiteEn);
  } else {
    return base::FeatureList::IsEnabled(kPageInfoAboutThisSiteNonEn);
  }
}

const base::Feature kPageInfoAboutThisSiteEn{"PageInfoAboutThisSiteEn",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kPageInfoAboutThisSiteNonEn{
    "PageInfoAboutThisSiteNonEn", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kShowSampleContent{&kPageInfoAboutThisSiteEn,
                                                  "ShowSampleContent", false};

const base::Feature kPageInfoAboutThisSiteMoreInfo{
    "PageInfoAboutThisSiteMoreInfo", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAboutThisSiteBanner{"AboutThisSiteBanner",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kPageInfoHistoryDesktop{"PageInfoHistoryDesktop",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPageInfoHideSiteSettings{
    "PageInfoHideSiteSettings", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace page_info
