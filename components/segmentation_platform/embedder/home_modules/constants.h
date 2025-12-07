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
inline constexpr char kAppBundleAppsInstalledCount[] =
    "app_bundle_apps_installed_count";
inline constexpr char kIsDefaultBrowserChromeIos[] =
    "is_default_browser_chrome_ios";
inline constexpr char kNumPriceDropsInShoppingList[] =
    "num_price_drops_in_shopping_list";

// Input Context keys for emphemeral android modules.
const char kIsUserSignedIn[] = "is_user_signed_in";
const char kShouldShowNonRoleManagerDefaultBrowserPromo[] =
    "should_show_non_role_manager_default_browser_promo";
const char kHasDefaultBrowserPromoShownInOtherSurface[] =
    "has_default_browser_promo_shown_in_other_surface";
const char kDefaultBrowserPromoShownCount[] = "default_browser_shown_count";
const char kTabGroupExists[] = "tab_group_exists";
const char kNumberOfTabs[] = "number_of_tabs";
const char kTabGroupPromoShownCount[] = "tab_group_shown_count";
const char kEducationalTipShownCount[] = "educational_tip_shown_count";
const char kSyncedTabGroupExists[] = "synced_tab_group_exists";
const char kTabGroupSyncPromoShownCount[] = "tab_group_sync_shown_count";
const char kCountOfClearingBrowsingData[] = "count_of_clearing_browsing_data";
const char kCountOfClearingBrowsingDataThroughQuickDelete[] =
    "count_of_clearing_browsing_data_through_quick_delete";
const char kQuickDeletePromoShownCount[] = "quick_delete_shown_count";
const char kIsEligibleToHistoryOptIn[] = "is_eligible_to_history_opt_in";
const char kHistorySyncPromoShownCount[] = "history_sync_promo_shown_count";
const char kIsEligibleToTipsOptIn[] = "is_eligible_to_tips_opt_in";
const char kTipsNotificationsPromoShownCount[] =
    "tips_notifications_promo_shown_count";

const char kAuxiliarySearchAvailable[] = "auxiliary_search_available";

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
inline constexpr char kAppBundlePromoEphemeralModule[] =
    "app_bundle_promo_ephemeral_module";
inline constexpr char kDefaultBrowserPromoEphemeralModule[] =
    "default_browser_promo_ephemeral_module";

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
inline constexpr char kDefaultBrowserPromo[] = "DefaultBrowserPromo";
inline constexpr char kTabGroupPromo[] = "TabGroupPromo";
inline constexpr char kTabGroupSyncPromo[] = "TabGroupSyncPromo";
inline constexpr char kQuickDeletePromo[] = "QuickDeletePromo";
inline constexpr char kHistorySyncPromo[] = "HistorySyncPromo";
inline constexpr char kTipsNotificationsPromo[] = "TipsNotificationsPromo";

// General limits for emphemeral android modules.
// This controls the display frequency limit for the general educational tip
// card on Android. It can be shown at most once within the number of days
// specified by this parameter.
inline constexpr int KDaysToShowEphemeralCardOnce = 3;
// This defines the display frequency limit for each educational tip card on
// Android. Each card can be shown at most once within the number of days
// specified by this parameter.
inline constexpr int KDaysToShowEachEphemeralCardOnce = 7;
// The maximum visibility count for each educational tip card on Android
// (excluding the default browser promo card).
inline constexpr int kSingleEphemeralCardMaxImpressions = 10;

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
