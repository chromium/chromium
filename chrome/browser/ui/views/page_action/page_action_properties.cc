// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_properties.h"

#include "base/check_is_test.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"

namespace page_actions {
namespace {

constexpr PageActionProperties kDefaultConfig;

constexpr auto kPageActionProperties =
    base::MakeFixedFlatMap<actions::ActionId, PageActionProperties>({
        {kActionSidePanelShowLensOverlayResults, {"LensOverlay", true}},
        {kActionShowTranslate, {"Translate", true}},
        {kActionShowMemorySaverChip, {"MemorySaver", true}},
        {kActionShowIntentPicker, {"IntentPicker", true}},
        {kActionZoomNormal, {"Zoom", true}},
        {kActionOffersAndRewardsForPage, {"PaymentsOfferNotification", true}},
    });

}  // namespace

const PageActionProperties& GetPageActionProperties(
    actions::ActionId page_action_id) {
  // In unit tests, `page_action_id` may not be in `kPageActionProperties`.
  if (!kPageActionProperties.contains(page_action_id)) {
    CHECK_IS_TEST();
    return kDefaultConfig;
  }

  return kPageActionProperties.at(page_action_id);
}

}  // namespace page_actions
