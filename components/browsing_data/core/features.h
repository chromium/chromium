// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_
#define COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace browsing_data::features {

// Pipes down the BrowsingDataModel to power site settings on Android.
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kBrowsingDataModel);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Enables a revamped Delete Browsing Data dialog. This includes UI changes,
// updates to history counter logic and removal of the bulk password deletion
// option from the dialog.
BASE_DECLARE_FEATURE(kDbdRevampDesktop);

// When enabled, calls to browsingData.removePasswords extension API would
// result in an error response instead of a console warning.
BASE_DECLARE_FEATURE(kPasswordRemovalExtensionErrorKillSwitch);

// Enables the Browsing History Actor Integration M1 feature. This feature
// introduces new ui and functionality related to the history integration in
// Chrome.
BASE_DECLARE_FEATURE(kBrowsingHistoryActorIntegrationM1);

// Whether Browsing History Actor Integration M1 or any dependent feature is
// enabled.
bool IsBrowsingHistoryActorIntegrationM1Enabled();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}  // namespace browsing_data::features

#endif  // COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_
