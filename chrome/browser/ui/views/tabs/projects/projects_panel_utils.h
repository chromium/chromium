// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_

class Profile;

namespace projects_panel {

// Returns whether the Projects Panel and its entrypoints should be visible in
// the UI for the profile.
bool IsProjectsPanelVisibleForProfile(Profile* profile);

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_UTILS_H_
