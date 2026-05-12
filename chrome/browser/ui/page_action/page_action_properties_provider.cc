// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_properties_provider.h"

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"

namespace {
constexpr auto kPageActionProperties = base::MakeFixedFlatMap<
    actions::ActionId,
    page_actions::PageActionProperties>({
    {
        kActionAiMode,
        {
            .histogram_name = "AiMode",
            .exempt_from_omnibox_suppression = true,
            .type = PageActionIconType::kAiMode,
            .element_identifier = kAiModePageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kCoreSiteUtility,
        },
    },
    {
        kActionIndigo,
        {
            .histogram_name = "Indigo",
            .type = PageActionIconType::kIndigo,
            .element_identifier = kIndigoPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kContextualCue,
        },
    },

    {
        kActionSidePanelShowLensOverlayResults,
        {
            .histogram_name = "LensOverlay",
            .type = PageActionIconType::kLensOverlay,
            .priority =
                page_actions::PageActionPriorityCategory::kDiscoveryNudge,
        },
    },
    {
        kActionLensOverlayHomework,
        {
            .histogram_name = "LensOverlayHomework",
            .type = PageActionIconType::kLensOverlayHomework,
            .element_identifier = kLensOverlayHomeworkPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kDiscoveryNudge,
        },
    },
    {
        kActionShowTranslate,
        {
            .histogram_name = "Translate",
            .type = PageActionIconType::kTranslate,
            .element_identifier = kTranslatePageActionElementId,
        },
    },
    {
        kActionShowMemorySaverChip,
        {
            .histogram_name = "MemorySaver",
            .type = PageActionIconType::kMemorySaver,
            .element_identifier = kMemorySaverChipElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kPrivacySecurity,
        },
    },
    {
        kActionShowJsOptimizationsIcon,
        {
            .histogram_name = "JsOptimizations",
            .type = PageActionIconType::kJsOptimizations,
            .element_identifier = kJsOptimizationsIconElementId,
        },
    },
    {
        kActionRecordReplay,
        {
            .histogram_name = "RecordReplay",
            .type = PageActionIconType::kRecordReplay,
            .element_identifier = kRecordButtonElementId,
        },
    },
    {
        kActionShowIntentPicker,
        {
            .histogram_name = "IntentPicker",
            .type = PageActionIconType::kIntentPicker,
            .element_identifier = kIntentPickerPageActionElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kCoreSiteUtility,
        },
    },
    {
        kActionZoomNormal,
        {
            .histogram_name = "Zoom",
            .type = PageActionIconType::kZoom,
            .element_identifier = kActionItemZoomElementId,
        },
    },
    {
        kActionSidePanelShowReadAnything,
        {
            .histogram_name = "ReadingMode",
            .type = PageActionIconType::kReadingMode,
            .priority =
                page_actions::PageActionPriorityCategory::kCoreSiteUtility,
        },
    },
    {
        kActionOffersAndRewardsForPage,
        {
            .histogram_name = "PaymentsOfferNotification",
            .type = PageActionIconType::kPaymentsOfferNotification,
            .element_identifier = kOfferNotificationChipElementId,
        },
    },
    {
        kActionShowFileSystemAccess,
        {
            .histogram_name = "ShowFileSystemAccess",
            .type = PageActionIconType::kFileSystemAccess,
            .element_identifier = kFileSystemPageActionElementId,
        },
    },
    {
        kActionInstallPwa,
        {
            .histogram_name = "PwaInstall",
            .type = PageActionIconType::kPwaInstall,
            .element_identifier = kInstallPwaElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kCoreSiteUtility,
        },
    },
    {
        kActionCommercePriceInsights,
        {
            .histogram_name = "PriceInsights",
            .type = PageActionIconType::kPriceInsights,
            .element_identifier = kPriceInsightsChipElementId,
            .priority = page_actions::PageActionPriorityCategory::kUnknown,
        },
    },
    {
        kActionCommerceDiscounts,
        {
            .histogram_name = "Discounts",
            .type = PageActionIconType::kDiscounts,
            .element_identifier = kDiscountsChipElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kDiscoveryNudge,
        },
    },
    {
        kActionShowPasswordsBubbleOrPage,
        {
            .histogram_name = "ManagePasswords",
            .type = PageActionIconType::kManagePasswords,
            .element_identifier = kPasswordsOmniboxKeyIconElementId,
        },
    },
    {
        kActionShowCollaborationRecentActivity,
        {
            .histogram_name = "ShowCollaborationRecentActivity",
            .type = PageActionIconType::kCollaborationMessaging,
            .element_identifier =
                kCollaborationMessagingPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kDiscoveryNudge,
        },
    },
    {
        kActionAutofillMandatoryReauth,
        {
            .histogram_name = "MandatoryReauth",
            .type = PageActionIconType::kMandatoryReauth,
            .element_identifier = kAutofillMandatoryReauthPageActionElementId,
        },
    },
    {
        kActionFind,
        {
            .histogram_name = "Find",
            .type = PageActionIconType::kFind,
            .element_identifier = kFindPageActionElementId,
        },
    },
    {
        kActionShowCookieControls,
        {
            .histogram_name = "CookieControls",
            .type = PageActionIconType::kCookieControls,
            .element_identifier = kCookieControlsIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kPrivacySecurity,
        },
    },
    {
        kActionShowAddressesBubbleOrPage,
        {
            .histogram_name = "AddressAutofill",
            .type = PageActionIconType::kAutofillAddress,
            .element_identifier = kAutofillAddressPageActionElementId,
        },
    },
    {
        kActionVirtualCardEnroll,
        {
            .histogram_name = "VirtualCardEnroll",
            .type = PageActionIconType::kVirtualCardEnroll,
            .element_identifier = kAutofillVirtualCardEnrollPageActionElementId,
        },
    },
    {
        kActionFilledCardInformation,
        {.histogram_name = "FilledCardInformation",
         .type = PageActionIconType::kFilledCardInformation,
         .element_identifier =
             kAutofillFilledCardInformationPageActionElementId},
    },
    {
        kActionShowPaymentsBubbleOrPage,
        {
            .histogram_name = "SavePayments",
            // This action id corresponds to both `kSaveCard` and
            // `kSaveIban` page action icon types. Since the framework only
            // supports 1:1 mapping of `ActionId`<->`PageActionIconType`,
            // and since `PageActionIconType` will be removed as an
            // identifier for page actions post migration, we choose to only
            // represent `kSaveCard` as the corresponding
            // `PageActionIconType` for `kActionShowPaymentsBubbleOrPage`.
            //
            // This peculiarity is handled well in all flows that rely on
            // `ActionId`<->`PageActionIconType` conversions, except in
            // framework level metrics for individual page action icons.
            // Therefore, we should rely on feature level metrics for this
            // particular page action.
            .type = PageActionIconType::kSaveCard,
            .element_identifier = kAutofillSavePaymentsPageActionElementId,
        },
    },
    {
        kActionSidePanelShowContextualTasks,
        {
            .histogram_name = "ContextualSidePanel",
            .type = PageActionIconType::kContextualSidePanel,
            .element_identifier = kContextualTasksPageActionElementId,
        },
    },
    {
        kActionBookmarkThisTab,
        {
            .histogram_name = "BookmarksStar",
            .is_ephemeral = false,
            .type = PageActionIconType::kBookmarkStar,
            .element_identifier = kBookmarkStarViewElementId,
        },
    },
    {
        kActionFederation,
        {
            .histogram_name = "Federation",
            .type = PageActionIconType::kFederation,
            .element_identifier = kFederationElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kPrivacySecurity,
        },
    },
    {
        kActionGlicContextualCueing,
        {
            .histogram_name = "Glic",
            .type = PageActionIconType::kGlic,
        },
    },
    {
        kActionAnchoredContextualCue,
        {
            .histogram_name = "ContextualCueing",
            .type = PageActionIconType::kAnchoredContextualCue,
        },
    },
    {
        kActionWebAuthnAmbientSignin,
        {
            .histogram_name = "WebAuthnAmbientSignin",
            .type = PageActionIconType::kWebAuthnAmbientSignin,
            .element_identifier = kWebAuthnAmbientSigninIconElementId,
        },
    },
    {
        kActionAutofillPayment,
        {
            .histogram_name = "AutofillPayment",
            .type = PageActionIconType::kAutofillPayment,
            .element_identifier = kAutofillPaymentIconElementId,
        },
    },
});

constexpr bool CheckIgnoreFlagUsage() {
  for (const auto& [action_id, properties] : kPageActionProperties) {
    if (properties.exempt_from_omnibox_suppression &&
        action_id != kActionAiMode) {
      return false;
    }
  }
  return true;
}

// AI Mode page action is designed to be displayed by itself. Other page actions
// should avoid using this property unless there is a strong reason.
static_assert(
    CheckIgnoreFlagUsage(),
    "ignore_should_hide_page_actions should only be used by kActionAiMode");

}  // namespace

namespace page_actions {

PageActionPropertiesProvider::PageActionPropertiesProvider() = default;
PageActionPropertiesProvider::~PageActionPropertiesProvider() = default;

bool PageActionPropertiesProvider::Contains(actions::ActionId action_id) const {
  return kPageActionProperties.contains(action_id);
}

const PageActionProperties& PageActionPropertiesProvider::GetProperties(
    actions::ActionId action_id) const {
  CHECK(Contains(action_id));
  return kPageActionProperties.at(action_id);
}

}  // namespace page_actions
