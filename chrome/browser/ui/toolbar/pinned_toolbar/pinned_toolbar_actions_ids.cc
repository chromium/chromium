// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"

namespace pinned_toolbar_actions {

ui::ElementIdentifier GetElementIdentifierForAction(actions::ActionId id) {
  switch (id) {
    case kActionSendSharedTabGroupFeedback:
      return kSharedTabGroupFeedbackElementId;
    case kActionSidePanelShowComments:
      return kSharedTabGroupCommentsActionElementId;
    case kActionSidePanelShowLensOverlayResults:
      return kPinnedToolbarActionShowSidePanelLensOverlayResultsElementId;
    case kActionSidePanelShowBookmarks:
      return kPinnedToolbarActionShowSidePanelBookmarksElementId;
    case kActionSendTabToSelf:
      return kPinnedToolbarActionSendTabToSelfElementId;
    case kActionSidePanelShowContextualTasks:
      return kPinnedToolbarActionShowSidePanelContextualTasksElementId;
    default:
      return ui::ElementIdentifier();
  }
}

}  // namespace pinned_toolbar_actions
