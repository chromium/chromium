
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_properties.h"

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"

namespace page_actions {

namespace {

constexpr auto kPageActionControllerProperties =
    base::MakeFixedFlatMap<actions::ActionId, PageActionControllerProperties>({
        {
            kActionSidePanelShowLensOverlayResults,
            {
                .histogram_name = "LensOverlay",
                .is_ephemeral = true,
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            kActionShowTranslate,
            {
                .histogram_name = "Translate",
                .is_ephemeral = true,
                .type = PageActionIconType::kTranslate,
            },
        },
        {
            kActionShowMemorySaverChip,
            {
                .histogram_name = "MemorySaver",
                .is_ephemeral = true,
                .type = PageActionIconType::kMemorySaver,
            },
        },
        {
            kActionShowIntentPicker,
            {
                .histogram_name = "IntentPicker",
                .is_ephemeral = true,
                .type = PageActionIconType::kIntentPicker,
            },
        },
        {
            kActionZoomNormal,
            {
                .histogram_name = "Zoom",
                .is_ephemeral = true,
                .type = PageActionIconType::kZoom,
            },
        },
        {
            kActionOffersAndRewardsForPage,
            {
                .histogram_name = "PaymentsOfferNotification",
                .is_ephemeral = true,
                .type = PageActionIconType::kPaymentsOfferNotification,
            },
        },
    });
}  // namespace

const PageActionControllerPropertiesMap& GetPageActionControllerProperties() {
  return kPageActionControllerProperties;
}

}  // namespace page_actions
