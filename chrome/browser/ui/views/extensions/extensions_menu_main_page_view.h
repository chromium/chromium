// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_

#include "chrome/browser/ui/views/extensions/extensions_menu_page_view.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
class ToggleButton;
}

class Browser;
class ExtensionsMenuNavigationHandler;
class ToolbarActionsModel;

// The main view of the extensions menu.
class ExtensionsMenuMainPageView : public ExtensionsMenuPageView {
 public:
  explicit ExtensionsMenuMainPageView(
      Browser* browser,
      ExtensionsMenuNavigationHandler* navigation_handler);
  ~ExtensionsMenuMainPageView() override = default;
  ExtensionsMenuMainPageView(const ExtensionsMenuMainPageView&) = delete;
  const ExtensionsMenuMainPageView& operator=(
      const ExtensionsMenuMainPageView&) = delete;

  void OnToggleButtonPressed();

  // ExtensionsMenuPageView:
  void Update() override;

 private:
  content::WebContents* GetActiveWebContents() const;

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsMenuNavigationHandler> navigation_handler_;
  const raw_ptr<ToolbarActionsModel> toolbar_model_;

  // Subheader.
  raw_ptr<views::Label> subheader_subtitle_;
  raw_ptr<views::ToggleButton> site_settings_toggle_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsMenuMainPageView,
                   ExtensionsMenuPageView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
