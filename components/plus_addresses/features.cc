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

#if BUILDFLAG(IS_ANDROID)
// When enabled, the user is shown the GMS core plus address management activity
// instead of the web page in a Chrome custom tab.
BASE_FEATURE(kPlusAddressAndroidOpenGmsCoreManagementPage,
             "PlusAddressAndroidOpenGmsCoreManagementPage",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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

// When enabled, plus address creation is offered on all email fields that are
// not a username field - even if they are on a login form or a change password
// form.
// Intended as a killswitch to protect against unexpected behavior.
// TODO: crbug.com/355398505 - clean up.
BASE_FEATURE(kPlusAddressOfferCreationOnAllNonUsernameFields,
             "PlusAddressOfferCreationOnAllNonUsernameFields",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, focusing on a username field will show plus address suggestion
// alongside the autocomplete suggestions.
// TODO(crbug.com/360791229): Remove when launched.
BASE_FEATURE(kPlusAddressSuggestionsOnUsernameFields,
             "PlusAddressSuggestionsOnUsernameFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace plus_addresses::features
