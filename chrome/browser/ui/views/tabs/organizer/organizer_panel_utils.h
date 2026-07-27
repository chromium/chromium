// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_UTILS_H_

class Profile;

namespace organizer_panel {

// Returns whether the Organizer Panel and its entrypoints should be visible in
// the UI for the profile.
bool IsOrganizerPanelVisibleForProfile(Profile* profile);

}  // namespace organizer_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_UTILS_H_
