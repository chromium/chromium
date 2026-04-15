// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"

// static
ui::ElementIdentifier PinnedToolbarActions::GetElementIdentifierForAction(
    actions::ActionId id) {
  switch (id) {
    case kActionSendSharedTabGroupFeedback:
      return kSharedTabGroupFeedbackElementId;
    case kActionSidePanelShowComments:
      return kSharedTabGroupCommentsActionElementId;
    case kActionSidePanelShowLensOverlayResults:
      return kPinnedToolbarActionShowSidePanelLensOverlayResultsElementId;
    case kActionSidePanelShowBookmarks:
      return kPinnedToolbarActionShowSidePanelBookmarksElementId;
    default:
      return ui::ElementIdentifier();
  }
}
