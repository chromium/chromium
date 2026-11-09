// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WINDOW_CREATION_H_
#define CHROME_BROWSER_UI_WAAP_WINDOW_CREATION_H_

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/waap/waap_utils.h"

class BrowserWindowInterface;

namespace waap {

// Creates a new browser window via `CreateBrowserWindow()` and records
// `creation_source` and the pre-creation timestamp on the window's
// `InitialWebUIWindowMetricsManager` (if present).
BrowserWindowInterface* CreateBrowserWindowAndRecordCreationSource(
    BrowserWindowCreateParams create_params,
    NewWindowCreationSource creation_source);

}  // namespace waap

#endif  // CHROME_BROWSER_UI_WAAP_WINDOW_CREATION_H_
