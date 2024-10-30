// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CONSTANTS_H_

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

namespace segmentation_platform {

// Input Context keys for emphemeral IOS modules.
inline constexpr char kIsNewUser[] = "is_new_user";
inline constexpr char kIsSynced[] = "is_sycned";
inline constexpr char kLacksEnhancedSafeBrowsing[] =
    "lacks_enhanced_safe_browsing";
inline constexpr char kLensAllowedByEnterprisePolicy[] =
    "lens_allowed_by_enterprise_policy";
inline constexpr char kPasswordManagerAllowedByEnterprisePolicy[] =
    "password_manager_allowed_by_enterprise_policy";
inline constexpr char kEnhancedSafeBrowsingAllowedByEnterprisePolicy[] =
    "enhanced_safe_browsing_allowed_by_enterprise_policy";
inline constexpr char kIsPhoneFormFactor[] = "is_phone_form_factor";
inline constexpr char kLensNotUsedRecently[] = "lens_not_used_recently";
inline constexpr char kDidNotSeeAddressBarPositionChoiceScreen[] =
    "did_not_see_address_bar_position_choice_screen";
inline constexpr char kNoSavedPasswords[] = "no_saved_passwords";
inline constexpr char kDidNotUsePasswordAutofill[] =
    "did_not_use_password_autofill";
inline constexpr char kSendTabInfobarReceivedInLastSession[] =
    "send_tab_infobar_received_in_last_session";

// Input Context keys for emphemeral android modules.
const char kIsDefaultBrowserChrome[] = "is_default_browser_chrome";
const char kHasDefaultBrowserPromoReachedLimitInRoleManager[] =
    "has_default_browser_promo_reached_limit_in_role_manager";
const char kHaveTabGroups[] = "have_tab_groups";
const char kNumberOfTabs[] = "number_of_tabs";
const char kHaveSyncedTabGroups[] = "have_synced_tab_groups";
const char kHaveClearedBrowsingDataInThirtyDays[] =
    "have_cleared_browsing_data_in_thirty_days";
const char kHaveUsedIncognito[] = "have_used_incognito";

// Placeholder output label for segmentation model executor.
inline constexpr char kPlaceholderEphemeralModuleLabel[] = "placeholder_module";

// Labels for emphemeral IOS modules.
inline constexpr char kPriceTrackingNotificationPromo[] =
    "price_tracking_promo";
inline constexpr char kAddressBarPositionEphemeralModule[] =
    "address_bar_position_ephemeral_module";
inline constexpr char kAutofillPasswordsEphemeralModule[] =
    "autofill_passwords_ephemeral_module";
inline constexpr char kEnhancedSafeBrowsingEphemeralModule[] =
    "enhanced_safe_browsing_ephemeral_module";
inline constexpr char kSavePasswordsEphemeralModule[] =
    "save_passwords_ephemeral_module";
inline constexpr char kLensEphemeralModule[] = "lens_ephemeral_module";
inline constexpr char kSendTabNotificationPromo[] = "send_tab_promo";

// Variation labels for emphemeral IOS modules.
// Lens variation labels
inline constexpr char kLensEphemeralModuleSearchVariation[] =
    "lens_ephemeral_module_search_variation";
inline constexpr char kLensEphemeralModuleShopVariation[] =
    "lens_ephemeral_module_shop_variation";
inline constexpr char kLensEphemeralModuleTranslateVariation[] =
    "lens_ephemeral_module_translate_variation";
inline constexpr auto kLensEphemeralModuleVariationLabels =
    base::MakeFixedFlatSet<std::string_view>({
        kLensEphemeralModuleSearchVariation,
        kLensEphemeralModuleShopVariation,
        kLensEphemeralModuleTranslateVariation,
    });

// Labels for emphemeral android modules.
const char kDefaultBrowserPromo[] = "default_browser_promo";

// Commandline ASCII Switch key to indicate that the test module backend ranker
// should be used.
inline constexpr char kEphemeralModuleBackendRankerTestOverride[] =
    "test-ephemeral-module-ranker";

// Returns the `TipIdentifier` corresponding to the given output `label`.
// If the `label` is unknown, returns `TipIdentifier::kUnknown`.
TipIdentifier TipIdentifierForOutputLabel(std::string_view label);

// Returns the `std::string_view` output label corresponding to the given
// `TipIdentifier`. If the `identifier` is unknown, returns `std::nullopt`.
std::optional<std::string_view> OutputLabelForTipIdentifier(
    TipIdentifier identifier);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CONSTANTS_H_
