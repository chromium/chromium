// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_UTILS_H_

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;

// UI-agnostic logic for the contextual tasks ephemeral toolbar button.
namespace contextual_tasks {

// Returns whether the contextual tasks side panel is aligned to the right side
// of the browser window, taking its per-entry alignment override into account.
// Returns false if `browser` is null or has no profile.
bool IsContextualTasksSidePanelRightAligned(BrowserWindowInterface* browser);

// Records metrics for the ephemeral button being shown in either the active
// panel-open state (`is_panel_open` = true) or the collapsed ephemeral state
// (`is_panel_open` = false).
void RecordEphemeralButtonShown(bool is_panel_open = false);

// Records metrics for the ephemeral button's context menu being opened.
// `close_side_panel_option_visible` is whether the menu includes the
// "Close/Remove" item for the current task.
void RecordEphemeralButtonContextMenuShown(
    bool close_side_panel_option_visible);

// Records metrics for the ephemeral button being pinned from its context menu.
void RecordEphemeralButtonPinByContextMenu();

// Handles a press of the ephemeral button: toggles the contextual tasks side
// panel and records the corresponding metrics. `browser` must be non-null and
// have a `ContextualTasksPanelController`. `on_ephemeral_open`, if not null, is
// run synchronously right before the side panel is opened by a press that
// counts as an ephemeral (rather than a permanently pinned) button press.
void HandleEphemeralButtonPressed(BrowserWindowInterface* browser,
                                  base::OnceClosure on_ephemeral_open);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_UTILS_H_
