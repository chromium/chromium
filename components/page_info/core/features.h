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

// Enables the "About this site" section in Page Info.
extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale);
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteEn);
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteNonEn);

// Whether we show hard-coded content for some sites like https://example.com.
extern const base::FeatureParam<bool> kShowSampleContent;

// Shows a link with more info about a site in PageInfo.
// Use page_info::IsAboutThisSiteFeatureEnabled() instead of checking this flag
// directly.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteMoreInfo);

// Shows a placeholder when a description is missing. Only enable in combination
// with kPageInfoAboutThisSiteMoreInfo.
// Use page_info::IsDescriptionPlaceholderEnabled() instead of checking this
// flag directly.
BASE_DECLARE_FEATURE(kPageInfoAboutThisSiteDescriptionPlaceholder);

#if !BUILDFLAG(IS_ANDROID)
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
