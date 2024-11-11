// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_FEATURES_H_
#define COMPONENTS_PLUS_ADDRESSES_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace plus_addresses::features {

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressAcceptedFirstTimeCreateSurvey);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressAndroidOpenGmsCoreManagementPage);
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressDeclinedFirstTimeCreateSurvey);

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

// The amount of time before the client aborts a request to the plus address
// server.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kPlusAddressRequestTimeout;

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

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressOfferCreationIfPasswordFieldIsNotVisible);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressOfferCreationOnAllNonUsernameFields);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressOfferCreationOnSingleUsernameForms);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressParseExistingProfilesFromCreateResponse);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressPreallocation);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<int> kPlusAddressPreallocationMinimumSize;

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressRefinedPasswordFormClassification);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressUserCreatedMultiplePlusAddressesSurvey);

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kPlusAddressUserOnboardingEnabled);

}  // namespace plus_addresses::features

#endif  // COMPONENTS_PLUS_ADDRESSES_FEATURES_H_
