// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_

#include "components/contextual_tasks/public/contextual_task.h"
#include "ui/gfx/vector_icon_types.h"

namespace views {
class Button;
class View;
}  // namespace views

class Profile;

namespace projects_panel {

// Returns whether the Projects Panel and its entrypoints should be visible in
// the UI for the profile.
bool IsProjectsPanelVisibleForProfile(Profile* profile);

void ConfigureInkDropForButton(views::Button* view);

const gfx::VectorIcon& GetIconForThreadType(
    contextual_tasks::ThreadType thread_type);

// Returns whether this view is the first focusable view in the panel.
// Currently, this is the panel close button. This is used for accessibility
// reasons (e.g., to focus the 3-dot button in a tab group item view) when
// determining whether the user performed a reverse focus traversal.
bool IsFirstFocusableViewInPanel(views::View* view);

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
