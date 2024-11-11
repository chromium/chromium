// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace plus_addresses::features {

namespace {

constexpr char kEnterprisePlusAddressOAuthScopeName[] = "oauth-scope";
constexpr char kEnterprisePlusAddressServerUrlName[] = "server-url";
constexpr char kPlusAddressManagementUrlName[] = "manage-url";
constexpr char kPlusAddressLearnMoreUrlName[] = "learn-more";
constexpr char kPlusAddressRequestTimeoutName[] = "request-timeout";

}  // namespace

// When enabled, a HaTS survey is shown after the successful first time creation
// flow.
BASE_FEATURE(kPlusAddressAcceptedFirstTimeCreateSurvey,
             "PlusAddressAcceptedFirstTimeCreateSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, the user is shown the GMS core plus address management activity
// instead of the web page in a Chrome custom tab.
BASE_FEATURE(kPlusAddressAndroidOpenGmsCoreManagementPage,
             "PlusAddressAndroidOpenGmsCoreManagementPage",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, a HaTS survey is shown after the declined the first plus
// address creation flow.
BASE_FEATURE(kPlusAddressDeclinedFirstTimeCreateSurvey,
             "PlusAddressDeclinedFirstTimeCreateSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the enabled/disabled state of the experimental feature.
BASE_FEATURE(kPlusAddressesEnabled,
             "PlusAddressesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kEnterprisePlusAddressOAuthScope{
    &kPlusAddressesEnabled, kEnterprisePlusAddressOAuthScopeName, ""};
const base::FeatureParam<std::string> kEnterprisePlusAddressServerUrl{
    &kPlusAddressesEnabled, kEnterprisePlusAddressServerUrlName, ""};
const base::FeatureParam<std::string> kPlusAddressManagementUrl{
    &kPlusAddressesEnabled, kPlusAddressManagementUrlName, ""};
const base::FeatureParam<std::string> kPlusAddressLearnMoreUrl{
    &kPlusAddressesEnabled, kPlusAddressLearnMoreUrlName, ""};
const base::FeatureParam<base::TimeDelta> kPlusAddressRequestTimeout{
    &kPlusAddressesEnabled, kPlusAddressRequestTimeoutName, base::Seconds(5)};

// When enabled, plus addresses are supported within the context menu.
BASE_FEATURE(kPlusAddressFallbackFromContextMenu,
             "PlusAddressFallbackFromContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the user has an existing plus address for the current
// domain, address profile suggestions will be updated to reflect the plus
// address email instead of the stored one. Note that only profile emails
// matching the user's GAIA account will be replaced.
BASE_FEATURE(kPlusAddressFullFormFill,
             "PlusAddressFullFormFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the `PlusAddressSettingService` will be consulted on whether
// to offer plus address creation.
BASE_FEATURE(kPlusAddressGlobalToggle,
             "PlusAddressGlobalToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// When enabled, users that have accepted the legal notice will see a
// streamlined flow for creating plus addresses that never leaves the Autofill
// popup.
BASE_FEATURE(kPlusAddressInlineCreation,
             "PlusAddressInlineCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// When enabled, plus address creation is offered also on login forms if the
// password field is not visible.
BASE_FEATURE(kPlusAddressOfferCreationIfPasswordFieldIsNotVisible,
             "PlusAddressOfferCreationIfPasswordFieldIsNotVisible",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, plus address creation is offered on all email fields that are
// not a username field - even if they are on a login form or a change password
// form.
// Intended as a killswitch to protect against unexpected behavior.
// TODO: crbug.com/355398505 - clean up.
BASE_FEATURE(kPlusAddressOfferCreationOnAllNonUsernameFields,
             "PlusAddressOfferCreationOnAllNonUsernameFields",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, we offer plus address creation on single username forms.
BASE_FEATURE(kPlusAddressOfferCreationOnSingleUsernameForms,
             "PlusAddressOfferCreationOnSingleUsernameForms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, we check whether the server response to a Create call returned
// information about existing profiles and return those as the parsing result.
BASE_FEATURE(kPlusAddressParseExistingProfilesFromCreateResponse,
             "PlusAddressParseExistingProfilesFromCreateResponse",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, plus addresses are preallocated to avoid having to query the
// server for every reserve call.
BASE_FEATURE(kPlusAddressPreallocation,
             "PlusAddressPreallocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The minimum number of locally stored pre-allocated plus addresses. If the
// number slips below this threshold, more are requested.
extern const base::FeatureParam<int> kPlusAddressPreallocationMinimumSize(
    &kPlusAddressPreallocation,
    "minimum-size",
    10);

// When enabled, plus address creation will be offered on forms that Password
// Manager classifies as login forms if those forms have a predicted field
// types that we believe not to be consistent with a login form - for example,
// FIRST_NAME or LAST_NAME.
// This therefore "refines" Password Manager predictions.
// TODO(crbug.com/364555384): Eventually, this should either be removed or
// integrated into Password Manager's own logic.
BASE_FEATURE(kPlusAddressRefinedPasswordFormClassification,
             "PlusAddressRefinedPasswordFormClassification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user creates a 3rd+ plus
// address.
BASE_FEATURE(kPlusAddressUserCreatedMultiplePlusAddressesSurvey,
             "PlusAddressUserCreatedMultiplePlusAddressesSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the plus address creation dialogs or bottom sheets include
// extended feature description and usage notice.
BASE_FEATURE(kPlusAddressUserOnboardingEnabled,
             "PlusAddressUserOnboardingEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace plus_addresses::features
