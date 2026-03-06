// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_

#include "components/contextual_tasks/public/contextual_task.h"
#include "ui/gfx/vector_icon_types.h"

namespace views {
class Button;
}  // namespace views

namespace projects_panel {

void ConfigureInkDropForButton(views::Button* view);

const gfx::VectorIcon& GetIconForThreadType(
    contextual_tasks::ThreadType thread_type);

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
