// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view_util.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"

namespace page_actions {

std::optional<SidePanelOpenTrigger> GetSidePanelOpenTriggerForPageAction(
    std::optional<actions::ActionId> action_id) {
  if (!action_id.has_value()) {
    return std::nullopt;
  }
  if (action_id == kActionSidePanelShowReadAnything) {
    return SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  }
  if (action_id == kActionSidePanelShowLensOverlayResults) {
    return SidePanelOpenTrigger::kSideSearchPageAction;
  }
  if (action_id == kActionSidePanelShowContextualTasks) {
    return SidePanelOpenTrigger::kContextualTasks;
  }
  return std::nullopt;
}

}  // namespace page_actions
