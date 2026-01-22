// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_

#include <memory>

namespace views {
class HighlightPathGenerator;
}  // namespace views

namespace projects_panel {

std::unique_ptr<views::HighlightPathGenerator>
GetListItemHighlightPathGenerator();

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
