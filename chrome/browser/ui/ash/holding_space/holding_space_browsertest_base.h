// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

class Profile;

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

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  // Returns the root window that newly created windows should be added to.
  static aura::Window* GetRootWindowForNewWindows();

  // Returns the currently active profile.
  Profile* GetProfile();

  // Shows holding space UI. This is a no-op if it's already showing.
  void Show();

  // Closes holding space UI. This is a no-op if it's already closed.
  void Close();

  // Returns true if holding space UI is showing, false otherwise.
  bool IsShowing();

  // Returns true if the holding space tray is showing in the shelf, false
  // otherwise.
  bool IsShowingInShelf();

  // Adds and returns an arbitrary download file to the holding space.
  HoldingSpaceItem* AddDownloadFile();

  // Adds and returns an arbitrary nearby share file to the holding space.
  HoldingSpaceItem* AddNearbyShareFile();

  // Adds and returns an arbitrary pinned file to the holding space.
  HoldingSpaceItem* AddPinnedFile();

  // Adds and returns an arbitrary screenshot file to the holding space.
  HoldingSpaceItem* AddScreenshotFile();

  // Adds and returns an arbitrary screen recording file to the holding space.
  HoldingSpaceItem* AddScreenRecordingFile();

  // Adds and returns a holding space item of the specified `type` backed by the
  // file at the specified `file_path`.
  HoldingSpaceItem* AddItem(Profile* profile,
                            HoldingSpaceItem::Type type,
                            const base::FilePath& file_path);

  // Removes the specified holding space `item`.
  void RemoveItem(const HoldingSpaceItem* item);

  // Creates a file at the root of the Downloads mount point with the specified
  // extension. If extension is omitted, the created file will have an extension
  // of `.txt`. Returns the file path of the created file.
  base::FilePath CreateFile(
      const base::Optional<std::string>& extension = base::nullopt);

  // Returns the collection of download chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetDownloadChips();

  // Returns the collection of pinned file chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetPinnedFileChips();

  // Returns the collection of screen capture views in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetScreenCaptureViews();

  // Returns the holding space tray in the shelf.
  views::View* GetTray();

  // Returns the view drawn on top of the holding space tray to indicate that
  // it is a drop target capable of handling the current drag payload.
  views::View* GetTrayDropTargetOverlay();

  // Getter for the holding space tray icons in the shelf.
  views::View* GetDefaultTrayIcon();
  views::View* GetPreviewsTrayIcon();

  // Returns whether the recent files bubble is shown.
  bool RecentFilesBubbleShown() const;

  // Requests lock screen, waiting to return until session state is locked.
  void RequestAndAwaitLockScreen();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_
