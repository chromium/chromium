// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

class BrowserActionsContainer;

class ExtensionActionTestHelperViews : public ExtensionActionTestHelper {
 public:
  ExtensionActionTestHelperViews(const ExtensionActionTestHelperViews&) =
      delete;
  ExtensionActionTestHelperViews& operator=(
      const ExtensionActionTestHelperViews&) = delete;
  ~ExtensionActionTestHelperViews() override;

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
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  gfx::Size GetToolbarActionSize() override;
  gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(int action_index) override;

 private:
  friend class ExtensionActionTestHelper;

  class TestToolbarActionsBarHelper;

  // Constructs a version of ExtensionActionTestHelperViews that does not own
  // the BrowserActionsContainer it tests.
  explicit ExtensionActionTestHelperViews(
      BrowserActionsContainer* browser_actions_container);
  // Constructs a version of ExtensionActionTestHelperViews given a
  // |test_helper| responsible for owning the BrowserActionsContainer.
  explicit ExtensionActionTestHelperViews(
      std::unique_ptr<TestToolbarActionsBarHelper> test_helper);

  std::unique_ptr<TestToolbarActionsBarHelper> test_helper_;

  // The associated BrowserActionsContainer. Not owned.
  BrowserActionsContainer* const browser_actions_container_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_VIEWS_H_
