// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"

namespace page_actions {

PageActionPropertiesProvider::PageActionPropertiesProvider() = default;
PageActionPropertiesProvider::~PageActionPropertiesProvider() = default;

const PageActionProperties& PageActionPropertiesProvider::GetProperties(
    actions::ActionId action_id) const {
  static const base::flat_map<actions::ActionId, PageActionProperties>
      kPageActionProperties({
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

  CHECK(kPageActionProperties.contains(action_id));
  return kPageActionProperties.at(action_id);
}

}  // namespace page_actions
