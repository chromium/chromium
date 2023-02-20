// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

namespace page_info {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPageInfoStoreInfo,
             "PageInfoStoreInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageInfoAboutThisSiteImprovedBottomSheet,
             "PageInfoAboutThisSiteImprovedBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale) {
  if (l10n_util::GetLanguage(locale) == "en") {
    return base::FeatureList::IsEnabled(kPageInfoAboutThisSiteEn);
  } else {
    return base::FeatureList::IsEnabled(kPageInfoAboutThisSiteNonEn);
  }
}

BASE_FEATURE(kPageInfoAboutThisSiteNewIcon,
             "PageInfoAboutThisSiteNewIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoAboutThisSiteEn,
             "PageInfoAboutThisSiteEn",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageInfoAboutThisSiteNonEn,
             "PageInfoAboutThisSiteNonEn",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPageInfoAboutThisSiteNonMsbb,
             "PageInfoAboutThisSiteNonMsbb",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kShowSampleContent{&kPageInfoAboutThisSiteEn,
                                                  "ShowSampleContent", false};

BASE_FEATURE(kPageInfoAboutThisSiteMoreInfo,
             "PageInfoAboutThisSiteMoreInfo",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPageInfoAboutThisSiteDescriptionPlaceholder,
             "PageInfoAboutThisSiteDescriptionPlaceholder",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPageInfoHistoryDesktop,
             "PageInfoHistoryDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoHideSiteSettings,
             "PageInfoHideSiteSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoCookiesSubpage,
             "PageInfoCookiesSubpage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageSpecificSiteDataDialog,
             "PageSpecificSiteDataDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif

}  // namespace page_info
