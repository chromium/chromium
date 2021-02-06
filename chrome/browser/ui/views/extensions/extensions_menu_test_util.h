// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

class Browser;
class ExtensionsMenuItemView;
class ExtensionsMenuView;
class ExtensionsToolbarContainer;

// An implementation of ExtensionActionTestHelper that works with the
// ExtensionsMenu (i.e., when features::kExtensionsToolbarMenu is enabled).
class ExtensionsMenuTestUtil : public ExtensionActionTestHelper {
 public:
  ExtensionsMenuTestUtil(Browser* browser, bool is_real_window);
  ExtensionsMenuTestUtil(const ExtensionsMenuTestUtil&) = delete;
  ExtensionsMenuTestUtil& operator=(const ExtensionsMenuTestUtil&) = delete;
  ~ExtensionsMenuTestUtil() override;

  // ExtensionActionTestHelper:
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
  bool HidePopup() override;
  void SetWidth(int width) override;
  ToolbarActionsBar* GetToolbarActionsBar() override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void WaitForExtensionsContainerLayout() override;
  std::unique_ptr<ExtensionActionTestHelper> CreateOverflowBar(
      Browser* browser) override;
  void LayoutForOverflowBar() override;
  // TODO(devlin): Some of these popup methods have a common implementation
  // between this and ExtensionActionTestHelperViews. It would make sense to
  // extract them (since they aren't dependent on the extension action UI
  // implementation).
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  gfx::Size GetToolbarActionSize() override;
  gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(int action_index) override;

 private:
  class Wrapper;

  // Returns the ExtensionsMenuItemView at the given |index| from the
  // |menu_view|.
  ExtensionsMenuItemView* GetMenuItemViewAtIndex(int index);

  // An override to allow test instances of the ExtensionsMenuView.
  // This has to be defined before |menu_view_| below.
  base::AutoReset<bool> scoped_allow_extensions_menu_instances_;

  std::unique_ptr<Wrapper> wrapper_;

  Browser* const browser_;
  ExtensionsToolbarContainer* extensions_container_ = nullptr;
  std::unique_ptr<ExtensionsMenuView> menu_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
