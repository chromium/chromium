// Copyright 2020 The Chromium Authors. All rights reserved.
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
extern const base::Feature kPageInfoHistory;
// Enables the store info row for Page Info.
extern const base::Feature kPageInfoStoreInfo;

// Used to experiment with different permission timeouts. The underlying feature
// itself is already launched.
extern const base::Feature kPageInfoDiscoverability;
#endif

// Enables the "About this site" section in Page Info.
extern bool IsAboutThisSiteFeatureEnabled(const std::string& locale);
extern const base::Feature kPageInfoAboutThisSiteEn;
extern const base::Feature kPageInfoAboutThisSiteNonEn;

// Whether we show hard-coded content for some sites like https://example.com.
extern const base::FeatureParam<bool> kShowSampleContent;

// Shows a link with more info about a site in PageInfo.
extern const base::Feature kPageInfoAboutThisSiteMoreInfo;

// Shows a placeholder when a description is missing. Only enable in combination
// with kPageInfoAboutThisSiteMoreInfo.
extern const base::Feature kPageInfoAboutThisSiteDescriptionPlaceholder;

// Enables the "About this site" banner.
extern const base::Feature kAboutThisSiteBanner;

#if !BUILDFLAG(IS_ANDROID)
// Enables the history section for Page Info on desktop.
extern const base::Feature kPageInfoHistoryDesktop;

// Hides site settings row.
extern const base::Feature kPageInfoHideSiteSettings;

// Enables Cookies Subpage. For implementation phase.
extern const base::Feature kPageInfoCookiesSubpage;

// Enables the new page specific site data dialog.
extern const base::Feature kPageSpecificSiteDataDialog;
#endif

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
