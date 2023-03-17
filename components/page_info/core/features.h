// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
#define COMPONENTS_PAGE_INFO_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace page_info {

#if BUILDFLAG(IS_ANDROID)
// Enables the history sub page for Page Info.
BASE_DECLARE_FEATURE(kPageInfoHistory);
// Enables the store info row for Page Info.
BASE_DECLARE_FEATURE(kPageInfoStoreInfo);
// Enables an improved "About this site" bottomsheet in Page Info.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteImprovedBottomSheet);
#endif

// Shows the new icon for the "About this site" section in Page Info
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteNewIcon);

// Enables the "About this site" section in Page Info.
extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale);

// Controls the feature for English and other languages that are enabled by
// default. Use IsAboutThisSiteFeatureEnabled() to check a specific language.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteEn);
// Controls the feature for languages that are not enabled by default yet.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteNonEn);

// Enables the "About this site" section for non-MSBB users.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteNonMsbb);

// Whether we show hard-coded content for some sites like https://example.com.
extern const base::FeatureParam<bool> kShowSampleContent;

// Shows a link with more info about a site in PageInfo.
// Use page_info::IsAboutThisSiteFeatureEnabled() instead of checking this flag
// directly.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteMoreInfo);

#if !BUILDFLAG(IS_ANDROID)
// Keeps the 'About this site' side panel open and updated on same tab
// navigations
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteKeepSidePanelOnSameTabNavs);

// Experiment with different secondary icons.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteSecondaryIcon);
// Parameter to select one of the icons from AboutThisSiteSeconaryIcon;
extern const base::FeatureParam<int> kAboutThisSiteSecondaryIconId;

// Enables the history section for Page Info on desktop.
BASE_DECLARE_FEATURE(kPageInfoHistoryDesktop);

// Hides site settings row.
BASE_DECLARE_FEATURE(kPageInfoHideSiteSettings);

// Enables Cookies Subpage. For implementation phase.
BASE_DECLARE_FEATURE(kPageInfoCookiesSubpage);

// Enables the new page specific site data dialog.
BASE_DECLARE_FEATURE(kPageSpecificSiteDataDialog);
#endif

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
