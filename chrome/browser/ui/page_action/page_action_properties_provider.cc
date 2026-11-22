// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_properties_provider.h"

#include <array>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"

namespace {
// LINT.IfChange(kPageActionProperties)
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
                page_actions::PageActionPriorityCategory::kStrategicPromotion,
        },
    },
    {
        kActionIndigo,
        {
            .histogram_name = "Indigo",
            .type = PageActionIconType::kIndigo,
            .element_identifier = kIndigoPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kProactiveSuggestion,
        },
    },
    {
        kActionMultistepFilter,
        {
            .histogram_name = "MultistepFilter",
            .type = PageActionIconType::kMultistepFilter,
            .element_identifier = kMultistepFilterPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kProactiveSuggestion,
        },
    },

    {
        kActionSidePanelShowLensOverlayResults,
        {
            .histogram_name = "LensOverlay",
            .type = PageActionIconType::kLensOverlay,
            .element_identifier = kLensOverlayPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kStrategicPromotion,
        },
    },
    {
        kActionLensOverlayHomework,
        {
            .histogram_name = "LensOverlayHomework",
            .type = PageActionIconType::kLensOverlayHomework,
            .element_identifier = kLensOverlayHomeworkPageActionIconElementId,
            .priority =
                page_actions::PageActionPriorityCategory::kStrategicPromotion,
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
                page_actions::PageActionPriorityCategory::kStrategicPromotion,
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
            .priority = page_actions::PageActionPriorityCategory::kQuietUtility,
        },
    },
    {
        kActionShowZoomBubble,
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
            .element_identifier = kReadAnythingPageActionElementId,
            .priority = page_actions::PageActionPriorityCategory::kQuietUtility,
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
            .priority = page_actions::PageActionPriorityCategory::kQuietUtility,
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
            .priority = page_actions::PageActionPriorityCategory::kQuietUtility,
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
            .priority = page_actions::PageActionPriorityCategory::kQuietUtility,
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
            .priority = page_actions::PageActionPriorityCategory::
                kSecurityTrustAndPermissions,
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
            // This action id is shared by both Save Card and Save IBAN flows.
            // Since the framework only supports a 1:1 mapping of
            // `ActionId`<->`PageActionIconType` (and `kSaveIban` is
            // deprecated), we represent `kSaveCard` as the corresponding
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
            .priority = page_actions::PageActionPriorityCategory::
                kSecurityTrustAndPermissions,
        },
    },
    {
        kActionGlicContextualCueing,
        {
            .histogram_name = "Glic",
            .type = PageActionIconType::kGlic,
            .element_identifier = kGlicContextualCueingPageActionIconElementId,
        },
    },
    {
        kActionAnchoredContextualCue,
        {
            .histogram_name = "ContextualCueing",
            .type = PageActionIconType::kAnchoredContextualCue,
            .element_identifier = kAnchoredContextualCuePageActionIconElementId,
        },
    },
    {
        kActionAutofillPayment,
        {
            .histogram_name = "AutofillPayment",
            .type = PageActionIconType::kAutofillPayment,
            .element_identifier = kAutofillPaymentIconElementId,
            .priority = page_actions::PageActionPriorityCategory::
                kSecurityTrustAndPermissions,
        },
    },
    {
        kActionShowPaymentsChurnedUsersBubble,
        {
            .histogram_name = "PaymentsChurnedUsers",
            .type = PageActionIconType::kPaymentsChurnedUsers,
            .element_identifier = kPaymentsChurnedUsersBubbleId,
        },
    },
    {
        kActionFakePageActionForDebug,
        {
            .histogram_name = "FakePageActionForDebug",
            .type = PageActionIconType::kFakePageActionForDebug,
            .element_identifier = kFakePageActionForDebugElementId,
            .priority = page_actions::PageActionPriorityCategory::
                kSecurityTrustAndPermissions,
        },
    },
    {
        kActionWalletReminderNotice,
        {
            .histogram_name = "WalletReminderNotice",
            .type = PageActionIconType::kWalletReminderNotice,
            .element_identifier = kPageActionWalletReminderNoticeElementId,
        },
    },
    {
        kActionGmailOtpOptIn,
        {
            .histogram_name = "GmailOtpOptIn",
            .type = PageActionIconType::kGmailOtpOptIn,
            .element_identifier = kPageActionGmailOtpOptInElementId,
        },
    },
    {
        kActionOnDeviceAi,
        {
            .histogram_name = "OnDeviceAi",
            .type = PageActionIconType::kOnDeviceAi,
            .element_identifier = kPageActionOnDeviceAiElementId,
        },
    },
});
// LINT.ThenChange(//components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom:PageActionId,//chrome/browser/ui/page_action/action_ids.h:kActionIds,//tools/metrics/histograms/metadata/page/histograms.xml:PageActionType)

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

constexpr bool CheckNoUserInteractionDefaultPriority() {
  for (const auto& [action_id, properties] : kPageActionProperties) {
    if (properties.priority ==
        page_actions::PageActionPriorityCategory::kUserInteraction) {
      return false;
    }
  }
  return true;
}

static_assert(
    CheckNoUserInteractionDefaultPriority(),
    "kUserInteraction priority should not be the default for any page action");

constexpr bool CheckAllActionIdsHaveProperties() {
  for (actions::ActionId action_id : page_actions::kActionIds) {
    if (!kPageActionProperties.contains(action_id)) {
      return false;
    }
  }
  return true;
}

static_assert(
    CheckAllActionIdsHaveProperties(),
    "Every ActionId in kActionIds must have an entry in kPageActionProperties");

constexpr bool CheckElementIdentifiersDefined() {
  for (const auto& [action_id, properties] : kPageActionProperties) {
    if (!properties.element_identifier) {
      return false;
    }
  }
  return true;
}

static_assert(CheckElementIdentifiersDefined(),
              "Every page action must define an element_identifier");

constexpr bool CheckElementIdentifiersUnique() {
  // TODO(crbug.com/587994323): This is O(n^2)...use std::set once Chromium
  // migrates to C++26.
  for (auto it1 = kPageActionProperties.begin();
       it1 != kPageActionProperties.end(); ++it1) {
    auto it2 = it1;
    for (++it2; it2 != kPageActionProperties.end(); ++it2) {
      if (it1->second.element_identifier == it2->second.element_identifier) {
        return false;
      }
    }
  }
  return true;
}

static_assert(CheckElementIdentifiersUnique(),
              "Each page action must have a unique element_identifier");

}  // namespace

namespace page_actions {

PageActionPropertiesProvider::PageActionPropertiesProvider() = default;
PageActionPropertiesProvider::~PageActionPropertiesProvider() = default;

bool PageActionPropertiesProvider::Contains(actions::ActionId action_id) const {
  return kPageActionProperties.contains(action_id);
}

// static
base::span<const ui::ElementIdentifier>
PageActionPropertiesProvider::GetAllElementIdentifiers() {
  static constexpr auto kElementIdentifiers = []() consteval {
    std::array<ui::ElementIdentifier, kPageActionProperties.size()> result;
    size_t index = 0;
    for (const auto& [action_id, properties] : kPageActionProperties) {
      result[index++] = properties.element_identifier;
    }
    return result;
  }();
  return kElementIdentifiers;
}

const PageActionProperties& PageActionPropertiesProvider::GetProperties(
    actions::ActionId action_id) const {
  CHECK(Contains(action_id));
  return kPageActionProperties.at(action_id);
}

}  // namespace page_actions
