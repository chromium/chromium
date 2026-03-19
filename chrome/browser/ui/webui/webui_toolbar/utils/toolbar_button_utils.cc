// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"

namespace webui_toolbar {

bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    toolbar_ui_api::mojom::ToolbarButtonType type) {
  switch (type) {
    case toolbar_ui_api::mojom::ToolbarButtonType::kSplitTabs:
      return browser_interface->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kPinSplitTabButton);
    case toolbar_ui_api::mojom::ToolbarButtonType::kHome:
      return browser_interface->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kShowHomeButton);
    case toolbar_ui_api::mojom::ToolbarButtonType::kUnspecified:
      NOTREACHED() << "Unexpected ToolbarButtonType::kUnspecified.";
  }
}

}  // namespace webui_toolbar
