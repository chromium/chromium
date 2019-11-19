// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_TEST_UTIL_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_TEST_UTIL_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/extensions/browser_action_test_util.h"

class BrowserActionsContainer;

class BrowserActionTestUtilViews : public BrowserActionTestUtil {
 public:
  BrowserActionTestUtilViews(const BrowserActionTestUtilViews&) = delete;
  BrowserActionTestUtilViews& operator=(const BrowserActionTestUtilViews&) =
      delete;
  ~BrowserActionTestUtilViews() override;

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
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  bool CanBeResized() override;

 private:
  friend class BrowserActionTestUtil;

  class TestToolbarActionsBarHelper;

  // Constructs a version of BrowserActionTestUtilViews that does not own the
  // BrowserActionsContainer it tests.
  explicit BrowserActionTestUtilViews(
      BrowserActionsContainer* browser_actions_container);
  // Constructs a version of BrowserActionTestUtilViews given a |test_helper|
  // responsible for owning the BrowserActionsContainer.
  explicit BrowserActionTestUtilViews(
      std::unique_ptr<TestToolbarActionsBarHelper> test_helper);

  std::unique_ptr<TestToolbarActionsBarHelper> test_helper_;

  // The associated BrowserActionsContainer. Not owned.
  BrowserActionsContainer* const browser_actions_container_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_TEST_UTIL_VIEWS_H_
