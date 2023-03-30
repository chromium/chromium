// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_SWITCHES_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_SWITCHES_H_

namespace companion::switches {

extern const char kDisableCheckUserPermissionsForCompanion[];

// Returns true if checking of the user's permissions to share page information
// with the Companion server should be ignored. Returns true only in tests.
bool ShouldOverrideCheckingUserPermissionsForCompanion();

}  // namespace companion::switches

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_SWITCHES_H_
