// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/features.h"

#include "base/containers/contains.h"
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

const char* default_langs[]{"en", "pt", "fr", "it", "nl", "de", "es"};

extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale) {
  std::string lang = l10n_util::GetLanguage(locale);
  if (base::Contains(default_langs, lang)) {
    return base::FeatureList::IsEnabled(kPageInfoAboutThisSite);
  }
  return base::FeatureList::IsEnabled(kPageInfoAboutThisSiteMoreLangs);
}

BASE_FEATURE(kPageInfoAboutThisSiteNewIcon,
             "PageInfoAboutThisSiteNewIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoAboutThisSite,
             "PageInfoAboutThisSite",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageInfoAboutThisSiteMoreLangs,
             "PageInfoAboutThisSiteMoreLangs",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kShowSampleContent{&kPageInfoAboutThisSite,
                                                  "ShowSampleContent", false};

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPageInfoAboutThisSiteKeepSidePanelOnSameTabNavs,
             "PageInfoAboutThisSiteKeepSidePanelOnSameTabNavs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoHistoryDesktop,
             "PageInfoHistoryDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoHideSiteSettings,
             "PageInfoHideSiteSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoCookiesSubpage,
             "PageInfoCookiesSubpage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageSpecificSiteDataDialog,
             "PageSpecificSiteDataDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif

}  // namespace page_info
