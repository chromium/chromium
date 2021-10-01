// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class ExtensionsToolbarButton;
class Browser;

class ExtensionsToolbarControls : public ToolbarIconContainerView {
 public:
  METADATA_HEADER(ExtensionsToolbarControls);

  explicit ExtensionsToolbarControls(
      Browser* browser,
      std::unique_ptr<ExtensionsToolbarButton> extensions_button,
      std::unique_ptr<ExtensionsToolbarButton> site_access_button);
  ExtensionsToolbarControls(const ExtensionsToolbarControls&) = delete;
  ExtensionsToolbarControls operator=(const ExtensionsToolbarControls&) =
      delete;
  ~ExtensionsToolbarControls() override;

  ExtensionsToolbarButton* extensions_button() const {
    return extensions_button_;
  }

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  ExtensionsToolbarButton* const site_access_button_;
  ExtensionsToolbarButton* const extensions_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTROLS_H_
