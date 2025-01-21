// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SAVED_TAB_GROUPS_UNSUPPORTED_SAVED_TAB_GROUPS_UNSUPPORTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SAVED_TAB_GROUPS_UNSUPPORTED_SAVED_TAB_GROUPS_UNSUPPORTED_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class SavedTabGroupsUnsupportedUI;

class SavedTabGroupsUnsupportedUIConfig
    : public content::DefaultWebUIConfig<SavedTabGroupsUnsupportedUI> {
 public:
  SavedTabGroupsUnsupportedUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISavedTabGroupsUnsupportedHost) {}
};

// The WebUI for chrome://saved-tab-groups-unsupported/. This WebUI is
// introduced for page URLs that are not supported for saved tab groups feature.
// The Sync server will receive the unsupported URL when a save tab group
// contains an unsupported URL. And the unsupported URL will be sent to other
// deices later.
class SavedTabGroupsUnsupportedUI : public content::WebUIController {
 public:
  explicit SavedTabGroupsUnsupportedUI(content::WebUI* web_ui);
  ~SavedTabGroupsUnsupportedUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SAVED_TAB_GROUPS_UNSUPPORTED_SAVED_TAB_GROUPS_UNSUPPORTED_UI_H_
