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
constexpr char kSyncWithEnterprisePlusAddressServerName[] = "sync-with-server";
constexpr char kEnterprisePlusAddressTimerDelayName[] = "timer-delay";
constexpr char kPlusAddressManagementUrlName[] = "manage-url";
constexpr char kPlusAddressLearnMoreUrlName[] = "learn-more";
constexpr char kPlusAddressExcludedSitesName[] = "excluded-sites";
constexpr char kPlusAddressErrorReportUrlName[] = "error-report-url";
constexpr char kDisableForForbiddenUsersName[] = "disable-for-forbidden-users";

}  // namespace

// When enabled, allows the use of affiliation data with plus addresses. This
// includes things like prefetching affiliation data, or suggesting plus
// addresses for affiliated domains.
BASE_FEATURE(kPlusAddressAffiliations,
             "PlusAddressAffiliations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the enabled/disabled state of the experimental feature.
BASE_FEATURE(kPlusAddressesEnabled,
             "PlusAddressesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kEnterprisePlusAddressOAuthScope{
    &kPlusAddressesEnabled, kEnterprisePlusAddressOAuthScopeName, ""};
const base::FeatureParam<std::string> kEnterprisePlusAddressServerUrl{
    &kPlusAddressesEnabled, kEnterprisePlusAddressServerUrlName, ""};
const base::FeatureParam<bool> kSyncWithEnterprisePlusAddressServer{
    &kPlusAddressesEnabled, kSyncWithEnterprisePlusAddressServerName, false};
const base::FeatureParam<base::TimeDelta> kEnterprisePlusAddressTimerDelay{
    &kPlusAddressesEnabled, kEnterprisePlusAddressTimerDelayName,
    base::Hours(24)};
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

// When enabled, the `PlusAddressSettingService` will be consulted on whether
// to offer plus address creation.
BASE_FEATURE(kPlusAddressGlobalToggle,
             "PlusAddressGlobalToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, we offer plus address creation on single username forms.
BASE_FEATURE(kPlusAddressOfferCreationOnSingleUsernameForms,
             "PlusAddressOfferCreationOnSingleUsernameForms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, `GoogleGroupsManager::IsFeatureEnabledForProfile` is used to
// check whether `kPlusAddressesEnabled` is enabled. Used as a killswitch.
// TODO: crbug.com/348575889 - Clean up.
BASE_FEATURE(kPlusAddressProfileAwareFeatureCheck,
             "PlusAddressProfileAwareFeatureCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, plus address refresh requests to the backend are supported.
BASE_FEATURE(kPlusAddressRefresh,
             "PlusAddressRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// When enabled, plus address settings on Desktop are nested under address
// autofill and the label for the autofill settings entry gains a subtitle.
BASE_FEATURE(kPlusAddressSettingsRefreshDesktop,
             "PlusAddressSettingsRefreshDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// When enabled, the plus address creation dialogs or bottom sheets include
// extended feature description and usage notice.
BASE_FEATURE(kPlusAddressUserOnboardingEnabled,
             "PlusAddressUserOnboardingEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill stops overriding single field form fill suggestions
// with plus address suggestions. Instead, it shows them together in the same
// context menu.
BASE_FEATURE(kPlusAddressAndSingleFieldFormFill,
             "PlusAddressAndSingleFieldFormFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, mobile manual fallbacks for addresses and passwords show plus
// address filling information.
BASE_FEATURE(kPlusAddressAndroidManualFallbackEnabled,
             "PlusAddressAndroidManualFallbackEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace plus_addresses::features
