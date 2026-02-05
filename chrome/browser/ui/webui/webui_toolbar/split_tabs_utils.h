/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_SPLIT_TABS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_SPLIT_TABS_UTILS_H_

#include "components/browser_apis/browser_controls/browser_controls_api_data_model.mojom.h"

class BrowserWindowInterface;

namespace content {
class WebUIDataSource;
}

namespace webui_toolbar {

// Represents the split state of the active tab.
struct TabSplitStatus {
  bool is_split = false;
  browser_controls_api::mojom::SplitTabActiveLocation location =
      browser_controls_api::mojom::SplitTabActiveLocation::kStart;

  bool operator==(const TabSplitStatus& other) const = default;
};

// Calculates the split status of the active tab.
TabSplitStatus ComputeTabSplitStatus(BrowserWindowInterface* browser_interface);

// Gets the pin state from user prefs.
bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    browser_controls_api::mojom::ToolbarButtonType type);

// Populates the WebUI data source with split tabs specific strings and initial
// state.
void PopulateSplitTabsDataSource(content::WebUIDataSource* source,
                                 BrowserWindowInterface* browser_interface);

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_SPLIT_TABS_UTILS_H_
