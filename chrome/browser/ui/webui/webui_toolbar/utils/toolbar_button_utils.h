// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_TOOLBAR_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_TOOLBAR_BUTTON_UTILS_H_

#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

class BrowserWindowInterface;

namespace webui_toolbar {

// Gets the pin state from user prefs.
bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    toolbar_ui_api::mojom::ToolbarButtonType type);

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_TOOLBAR_BUTTON_UTILS_H_
