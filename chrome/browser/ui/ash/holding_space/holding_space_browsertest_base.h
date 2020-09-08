// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceTestApi;

// Base class for holding space browser tests.
class HoldingSpaceBrowserTestBase : public InProcessBrowserTest {
 public:
  HoldingSpaceBrowserTestBase();
  ~HoldingSpaceBrowserTestBase() override;

  // Returns the root window that newly created windows should be added to.
  static aura::Window* GetRootWindowForNewWindows();

  // Shows holding space UI. This is a no-op if it's already showing.
  void Show();

  // Closes holding space UI. This is a no-op if it's already closed.
  void Close();

  // Returns true if holding space UI is showing, false otherwise.
  bool IsShowing();

  // Adds and returns an arbitrary pinned file to the holding space.
  HoldingSpaceItem* AddPinnedFile();

  // Adds and returns an arbitrary screenshot file to the holding space.
  HoldingSpaceItem* AddScreenshotFile();

  // Returns the collection of pinned file chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetPinnedFileChips();

  // Returns the collection of screenshot views in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetScreenshotViews();

 private:
  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir scoped_temp_dir_;
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_
