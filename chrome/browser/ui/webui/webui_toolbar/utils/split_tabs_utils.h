/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_SPLIT_TABS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_SPLIT_TABS_UTILS_H_

#include "components/browser_apis/browser_controls/browser_controls_api_data_model.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

class BrowserWindowInterface;

namespace content {
class WebUIDataSource;
}

namespace webui_toolbar {

// Represents the split state of the active tab.
struct TabSplitStatus {
  bool is_split = false;
  toolbar_ui_api::mojom::SplitTabActiveLocation location =
      toolbar_ui_api::mojom::SplitTabActiveLocation::kStart;

  bool operator==(const TabSplitStatus& other) const = default;
};

// Calculates the split status of the active tab.
TabSplitStatus ComputeTabSplitStatus(BrowserWindowInterface* browser_interface);

// Gets the pin state from user prefs.
bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    toolbar_ui_api::mojom::ToolbarButtonType type);

// Populates the WebUI data source with split tabs specific strings and initial
// state.
void PopulateSplitTabsDataSource(content::WebUIDataSource* source,
                                 BrowserWindowInterface* browser_interface);

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_SPLIT_TABS_UTILS_H_
