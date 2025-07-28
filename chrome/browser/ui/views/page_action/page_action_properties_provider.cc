// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"

namespace {
constexpr auto kPageActionProperties =
    base::MakeFixedFlatMap<actions::ActionId,
                           page_actions::PageActionProperties>({
        {
            kActionSidePanelShowLensOverlayResults,
            {
                .histogram_name = "LensOverlay",
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            kActionShowTranslate,
            {
                .histogram_name = "Translate",
                .type = PageActionIconType::kTranslate,
            },
        },
        {
            kActionShowMemorySaverChip,
            {
                .histogram_name = "MemorySaver",
                .type = PageActionIconType::kMemorySaver,
                .element_identifier = kMemorySaverChipElementId,
            },
        },
        {
            kActionShowIntentPicker,
            {
                .histogram_name = "IntentPicker",
                .type = PageActionIconType::kIntentPicker,
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
            },
        },
        {
            kActionInstallPwa,
            {
                .histogram_name = "PwaInstall",
                .type = PageActionIconType::kPwaInstall,
            },
        },
        {
            kActionCommercePriceInsights,
            {
                .histogram_name = "PriceInsights",
                .type = PageActionIconType::kPriceInsights,
                .element_identifier = kPriceInsightsChipElementId,
            },
        },
        {
            kActionCommerceDiscounts,
            {
                .histogram_name = "Discounts",
                .type = PageActionIconType::kDiscounts,
                .element_identifier = kDiscountsChipElementId,
            },
        },
        {
            kActionCommerceProductSpecifications,
            {
                .histogram_name = "ProductSpecifications",
                .type = PageActionIconType::kProductSpecifications,
                .element_identifier = kProductSpecificationsChipElementId,
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
            },
        },
        {
            kActionAutofillMandatoryReauth,
            {
                .histogram_name = "MandatoryReauth",
                .type = PageActionIconType::kMandatoryReauth,
            },
        },
    });
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
