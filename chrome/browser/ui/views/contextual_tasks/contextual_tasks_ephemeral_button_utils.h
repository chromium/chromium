// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_UTILS_H_

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;

// UI-agnostic logic for the contextual tasks ephemeral toolbar button, shared
// by the Views (`ContextualTasksButton`) and WebUI toolbar implementations so
// that they only need to render the button and forward input.
namespace contextual_tasks {

// Visual shape of the ephemeral button. The flat edge sides are physical (not
// mirrored in RTL), since they follow the physical side of the side panel.
enum class EphemeralButtonShape {
  kCircle,
  kFlatEdgeLeft,
  kFlatEdgeRight,
};

// Returns whether the contextual tasks side panel is aligned to the right side
// of the browser window, taking its per-entry alignment override into account.
// Returns false if `browser` is null or has no profile.
bool IsContextualTasksSidePanelRightAligned(BrowserWindowInterface* browser);

// Returns whether the ephemeral button is at the trailing edge when it is
// docked at one of the toolbar edges.
bool IsEphemeralButtonTrailing(BrowserWindowInterface* browser);

// Returns whether the ephemeral button should be a circle placed before the
// Glic, avatar and app menu buttons, rather than docked at a toolbar edge.
bool ShouldPlaceCircularButtonBeforeProfile(BrowserWindowInterface* browser);

// Returns the shape the ephemeral button should currently have.
EphemeralButtonShape GetEphemeralButtonShape(BrowserWindowInterface* browser);

// Records metrics for the ephemeral button being shown in either the active
// panel-open state (`is_panel_open` = true) or the collapsed ephemeral state
// (`is_panel_open` = false).
void RecordEphemeralButtonShown(bool is_panel_open = false);

// Handles a press of the ephemeral button: toggles the contextual tasks side
// panel and records the corresponding metrics. `browser` must be non-null and
// have a `ContextualTasksPanelController`. `on_ephemeral_open`, if not null, is
// run synchronously right before the side panel is opened by a press that
// counts as an ephemeral (rather than a permanently pinned) button press.
void HandleEphemeralButtonPressed(BrowserWindowInterface* browser,
                                  base::OnceClosure on_ephemeral_open);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_UTILS_H_
