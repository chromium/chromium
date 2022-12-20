// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_

class Browser;
class SidePanelRegistry;

namespace extensions {

// Returns the global side panel registry for this browser. This must be called
// after the browser's BrowserView has been created.
SidePanelRegistry* GetGlobalSidePanelRegistry(Browser* browser);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
