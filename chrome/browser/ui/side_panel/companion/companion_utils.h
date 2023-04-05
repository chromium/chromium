// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_

class Browser;

namespace companion {

// Returns true if necessary flags are enabled, browser is valid and default
// search engine is Google.
bool IsSearchWebInCompanionSidePanelSupported(const Browser* browser);

}  // namespace companion

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_UTILS_H_
