// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_FEATURES_H_
#define COMPONENTS_PLUS_ADDRESSES_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace plus_addresses::features {

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressAndroidEnhancedLoadingStatesEnabled);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressAndroidErrorStatesEnabled);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressAndroidManualFallbackEnabled);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressAndroidSettingsEntry);
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressBlocklistEnabled);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressesEnabled);

// Used to control the enterprise plus address feature's OAuth scope.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kEnterprisePlusAddressOAuthScope;

// The url that the enterprise uses to create plus addresses. Must be a valid
// GURL, such as `https://foo.example/`.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kEnterprisePlusAddressServerUrl;

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kPlusAddressManagementUrl;

// Url used to redirect the user to the feature description page.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kPlusAddressLearnMoreUrl;

// Used to exclude certain sites from PlusAddressService. Must be a
// comma-separated list of site names (eTLD+1).
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kPlusAddressExcludedSites;

// Url for user to report issues with plus addresses.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kPlusAddressErrorReportUrl;

// Used to disable this feature when requests to the server repeatedly fail with
// a 403.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<bool> kDisableForForbiddenUsers;

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressFallbackFromContextMenu);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressFullFormFill);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressGlobalToggle);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressInlineCreation);
#endif

#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressIOSErrorAndLoadingStatesEnabled);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressIOSManualFallbackEnabled);
#endif  // BUILDFLAG(IS_IOS)

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressOfferCreationOnAllNonUsernameFields);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressOfferCreationOnSingleUsernameForms);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressPreallocation);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<int> kPlusAddressPreallocationMinimumSize;

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressProfileAwareFeatureCheck);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressSuggestionRedesign);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<bool> kShowForwardingEmailInSuggestion;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressUpdatedErrorStatesInOnboardingModal);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressUserOnboardingEnabled);

}  // namespace plus_addresses::features

#endif  // COMPONENTS_PLUS_ADDRESSES_FEATURES_H_
