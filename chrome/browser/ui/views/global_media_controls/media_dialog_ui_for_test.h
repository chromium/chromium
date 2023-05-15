// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_UI_FOR_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_UI_FOR_TEST_H_

#include <string>
#include "base/functional/callback.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class Browser;
class MediaToolbarButtonView;

// A helper object for interacting with the Global Media Control dialog inside
// unit tests.
class MediaDialogUiForTest {
 public:
  // Ideally this constructor would just take a Browser* as a parameter, but in
  // subclasses of InProcessBrowserTest, the browser isn't available until
  // SetUp() is called, and the tests are executed before SetUp() returns.
  explicit MediaDialogUiForTest(base::RepeatingCallback<Browser*()> callback);

  MediaDialogUiForTest(const MediaDialogUiForTest&) = delete;
  MediaDialogUiForTest& operator=(const MediaDialogUiForTest&) = delete;
  ~MediaDialogUiForTest();

  // Gets the GMC toolbar icon, with the side-effect of laying out the browser
  // window if necessary.
  MediaToolbarButtonView* GetToolbarIcon();

  // Lays out the browser window if it hasn't already been laid out.
  void LayoutBrowserIfNecessary();

  // Clicks the GMC toolbar icon, which must already be visible.  Generally
  // should be followed by a call to WaitForDialogOpened().
  void ClickToolbarIcon();

  // Tests whether the GMC toolbar icon is visible.
  bool IsToolbarIconVisible();

  // Waits for the GMC toolbar icon to become visible, usually in response to
  // some action like starting media playback.  Returns true if the toolbar icon
  // is visible after waiting, or false after a timeout.
  [[nodiscard]] bool WaitForToolbarIconShown();

  // Waits for the GMC toolbar icon to be hidden.  Returns true if the toolbar
  // icon is hidden after waiting, or false after a timeout.
  [[nodiscard]] bool WaitForToolbarIconHidden();

  // Waits for the GMC dialog to be open, usually in response to the toolbar
  // icon being clicked using ClickToolbarIcon().  Returns true if the dialog is
  // open after waiting, or false after a timeout.
  [[nodiscard]] bool WaitForDialogOpened();

  bool IsDialogVisible();
  void WaitForDialogToContainText(const std::u16string& text);
  void WaitForItemCount(int count);
  void WaitForPictureInPictureButtonVisibility(bool visible);

 private:
  global_media_controls::MediaItemManager* GetItemManager() const;

  base::RepeatingCallback<Browser*()> browser_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_UI_FOR_TEST_H_
