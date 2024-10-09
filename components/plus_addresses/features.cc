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
constexpr char kPlusAddressExcludedSitesName[] = "excluded-sites";
constexpr char kPlusAddressErrorReportUrlName[] = "error-report-url";
constexpr char kDisableForForbiddenUsersName[] = "disable-for-forbidden-users";
constexpr char kShowForwardingEmailInSuggestionName[] = "show-forwarding-email";

}  // namespace

#if BUILDFLAG(IS_ANDROID)
// When enabled, mobile plus address creation bottom sheet shows enhanced UI for
// different plus address loading states.
BASE_FEATURE(kPlusAddressAndroidEnhancedLoadingStatesEnabled,
             "PlusAddressAndroidEnhancedLoadingStatesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, mobile plus address creation bottom sheet shows enhanced UI for
// different error states.
BASE_FEATURE(kPlusAddressAndroidErrorStatesEnabled,
             "PlusAddressAndroidErrorStatesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, mobile manual fallbacks for addresses and passwords show plus
// address filling information.
BASE_FEATURE(kPlusAddressAndroidManualFallbackEnabled,
             "PlusAddressAndroidManualFallbackEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the mobile autofill profiles fragment shows a link to open plus
// address management page.
BASE_FEATURE(kPlusAddressAndroidSettingsEntry,
             "PlusAddressAndroidSettingsEntry",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, Chrome will fetch the blocklist data using the Component
// Updater and employ that for blocking Plus Addresses. Otherwise, the blocklist
// information is sourced from a Finch parameter.
BASE_FEATURE(kPlusAddressBlocklistEnabled,
             "PlusAddressBlocklistEnabled",
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
const base::FeatureParam<std::string> kPlusAddressExcludedSites{
    &kPlusAddressesEnabled, kPlusAddressExcludedSitesName, ""};
const base::FeatureParam<std::string> kPlusAddressErrorReportUrl{
    &kPlusAddressesEnabled, kPlusAddressErrorReportUrlName, ""};
const base::FeatureParam<bool> kDisableForForbiddenUsers{
    &kPlusAddressesEnabled, kDisableForForbiddenUsersName, false};

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

#if BUILDFLAG(IS_IOS)
// When enabled, plus address creation bottom sheet shows enhanced UI for
// different error states as well as loading states on iOS.
BASE_FEATURE(kPlusAddressIOSErrorAndLoadingStatesEnabled,
             "PlusAddressIOSErrorAndLoadingStatesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, mobile manual fallbacks for addresses and passwords show plus
// address filling information.
BASE_FEATURE(kPlusAddressIOSManualFallbackEnabled,
             "PlusAddressIOSManualFallbackEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

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

// When enabled, `GoogleGroupsManager::IsFeatureEnabledForProfile` is used to
// check whether `kPlusAddressesEnabled` is enabled. Used as a killswitch.
// TODO: crbug.com/348575889 - Clean up.
BASE_FEATURE(kPlusAddressProfileAwareFeatureCheck,
             "PlusAddressProfileAwareFeatureCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, creation suggestions do not contain a label prior to the user
// acknowledging the notice.
BASE_FEATURE(kPlusAddressSuggestionRedesign,
             "PlusAddressSuggestionRedesign",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If set to `true`, then labels, when shown, contain information about the
// forwarding address.
const base::FeatureParam<bool> kShowForwardingEmailInSuggestion{
    &kPlusAddressSuggestionRedesign, kShowForwardingEmailInSuggestionName,
    false};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// When enabled, we show refined error states in the onboarding dialog on
// Desktop.
BASE_FEATURE(kPlusAddressUpdatedErrorStatesInOnboardingModal,
             "PlusAddressUpdatedErrorStatesInOnboardingModal",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// When enabled, the plus address creation dialogs or bottom sheets include
// extended feature description and usage notice.
BASE_FEATURE(kPlusAddressUserOnboardingEnabled,
             "PlusAddressUserOnboardingEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace plus_addresses::features
