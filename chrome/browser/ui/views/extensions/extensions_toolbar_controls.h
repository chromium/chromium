// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "extensions/browser/permissions_manager.h"

namespace content {
class WebContents;
}

class ExtensionsToolbarButton;
class ExtensionsRequestAccessButton;
class ToolbarActionViewController;

class ExtensionsToolbarControls : public ToolbarIconContainerView {
 public:
  METADATA_HEADER(ExtensionsToolbarControls);

  explicit ExtensionsToolbarControls(
      std::unique_ptr<ExtensionsToolbarButton> extensions_button,
      std::unique_ptr<ExtensionsToolbarButton> site_access_button,
      std::unique_ptr<ExtensionsRequestAccessButton> request_button);
  ExtensionsToolbarControls(const ExtensionsToolbarControls&) = delete;
  ExtensionsToolbarControls operator=(const ExtensionsToolbarControls&) =
      delete;
  ~ExtensionsToolbarControls() override;

  ExtensionsToolbarButton* extensions_button() const {
    return extensions_button_;
  }

  // Methods for testing.
  ExtensionsToolbarButton* site_access_button_for_testing() const {
    return site_access_button_;
  }
  ExtensionsRequestAccessButton* request_access_button_for_testing() const {
    return request_access_button_;
  }

  // Update the controls given `actions` and the user `site_setting` in the
  // `current_web_contents`.
  void UpdateControls(
      const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
      extensions::PermissionsManager::UserSiteSetting site_setting,
      content::WebContents* current_web_contents);

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  // Updates `site_access_button_` visibility given `actions` in `web_contents`.
  void UpdateSiteAccessButton(
      const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
      content::WebContents* web_contents);

  // Updates `request_access_button_` visibility given the user `site_setting`
  // and `actions` in `web_contents`.
  void UpdateRequestAccessButton(
      const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
      extensions::PermissionsManager::UserSiteSetting site_setting,
      content::WebContents* web_contents);

  const raw_ptr<ExtensionsRequestAccessButton> request_access_button_;
  const raw_ptr<ExtensionsToolbarButton> site_access_button_;
  const raw_ptr<ExtensionsToolbarButton> extensions_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
