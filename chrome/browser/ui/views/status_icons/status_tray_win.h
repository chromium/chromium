// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_

#include <windows.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "chrome/browser/status_icons/status_tray.h"

class StatusIconWin;

// A class that's responsible for increasing, if possible, the visibility
// of a status tray icon on the taskbar. The default implementation sends
// a task to a worker thread each time EnqueueChange is called.
class StatusTrayStateChangerProxy {
 public:
  virtual ~StatusTrayStateChangerProxy() {}

  // Called by StatusTrayWin to request upgraded visibility on the icon
  // represented by the |icon_id|, |window| pair.
  virtual void EnqueueChange(UINT icon_id, HWND window) = 0;
};

class StatusTrayWin : public StatusTray {
 public:
  StatusTrayWin();

  StatusTrayWin(const StatusTrayWin&) = delete;
  StatusTrayWin& operator=(const StatusTrayWin&) = delete;

  ~StatusTrayWin() override;

  void UpdateIconVisibilityInBackground(StatusIconWin* status_icon);

  // Exposed for testing.
  LRESULT CALLBACK
      WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

 protected:
  // Overriden from StatusTray:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(StatusTrayWinTest, EnsureVisibleTest);

  // Static callback invoked when a message comes in to our messaging window.
  static LRESULT CALLBACK
      WndProcStatic(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  UINT NextIconId();

  void SetStatusTrayStateChangerProxyForTest(
      std::unique_ptr<StatusTrayStateChangerProxy> proxy);

  // The unique icon ID we will assign to the next icon.
  UINT next_icon_id_;

  // The window class of |window_|.
  ATOM atom_;

  // The handle of the module that contains the window procedure of |window_|.
  HMODULE instance_;

  // The window used for processing events.
  HWND window_;

  // The message ID of the "TaskbarCreated" message, sent to us when we need to
  // reset our status icons.
  UINT taskbar_created_message_;

  // Manages changes performed on a background thread to manipulate visibility
  // of notification icons.
  std::unique_ptr<StatusTrayStateChangerProxy> state_changer_proxy_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_
