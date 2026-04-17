// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TOOLBAR_ACTIONS_IDS_H_
#define CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TOOLBAR_ACTIONS_IDS_H_

#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"

namespace pinned_toolbar_actions {

// Returns the ElementIdentifier for an action.
// PinnedToolbarActions::GetBubbleAnchor() should generally be used for
// anchoring to pinned toolbar action buttons, but if you want an action to have
// a specific ElementIdentifier, feel free to add it to this method.
ui::ElementIdentifier GetElementIdentifierForAction(actions::ActionId id);

}  // namespace pinned_toolbar_actions

#endif  // CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TOOLBAR_ACTIONS_IDS_H_
