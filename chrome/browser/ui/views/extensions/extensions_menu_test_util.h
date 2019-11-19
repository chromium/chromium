// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"

class Browser;
class ExtensionsMenuItemView;
class ExtensionsMenuView;
class ExtensionsToolbarContainer;

// An implementation of BrowserActionTestUtil that works with the ExtensionsMenu
// (i.e., when features::kExtensionsToolbarMenu is enabled).
class ExtensionsMenuTestUtil : public BrowserActionTestUtil {
 public:
  explicit ExtensionsMenuTestUtil(Browser* browser);

  ~ExtensionsMenuTestUtil() override;

  // BrowserActionTestUtil:
  int NumberOfBrowserActions() override;
  int VisibleBrowserActions() override;
  void InspectPopup(int index) override;
  bool HasIcon(int index) override;
  gfx::Image GetIcon(int index) override;
  void Press(int index) override;
  std::string GetExtensionId(int index) override;
  std::string GetTooltip(int index) override;
  gfx::NativeView GetPopupNativeView() override;
  bool HasPopup() override;
  gfx::Size GetPopupSize() override;
  bool HidePopup() override;
  bool ActionButtonWantsToRun(size_t index) override;
  void SetWidth(int width) override;
  ToolbarActionsBar* GetToolbarActionsBar() override;
  std::unique_ptr<BrowserActionTestUtil> CreateOverflowBar(
      Browser* browser) override;
  // TODO(devlin): Some of these popup methods have a common implementation
  // between this and BrowserActionTestUtilViews. It would make sense to
  // extract them (since they aren't dependent on the extension action UI
  // implementation).
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  bool CanBeResized() override;

 private:
  // Returns the ExtensionsMenuItemView at the given |index| from the
  // |menu_view|.
  ExtensionsMenuItemView* GetMenuItemViewAtIndex(int index);

  // An override to allow test instances of the ExtensionsMenuView.
  // This has to be defined before |menu_view_| below.
  base::AutoReset<bool> scoped_allow_extensions_menu_instances_;

  Browser* const browser_;
  ExtensionsToolbarContainer* const extensions_container_;
  std::unique_ptr<ExtensionsMenuView> menu_view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuTestUtil);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
