// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_

#include <windows.h>

#include <shellapi.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/status_icons/status_icon.h"

namespace gfx {
class Point;
}

namespace views {
class MenuRunner;
}

class StatusTrayWin;

class StatusIconWin : public StatusIcon {
 public:
  // Constructor which provides this icon's unique ID and messaging window.
  StatusIconWin(StatusTrayWin* tray, UINT id, HWND window, UINT message);

  StatusIconWin(const StatusIconWin&) = delete;
  StatusIconWin& operator=(const StatusIconWin&) = delete;

  ~StatusIconWin() override;

  // Handles a click event from the user - if |left_button_click| is true and
  // there is a registered observer, passes the click event to the observer,
  // otherwise displays the context menu if there is one.
  void HandleClickEvent(const gfx::Point& cursor_pos, bool left_button_click);

  // Handles a click on the balloon from the user.
  void HandleBalloonClickEvent();

  // Re-creates the status tray icon now after the taskbar has been created.
  void ResetIcon();

  UINT icon_id() const { return icon_id_; }
  HWND window() const { return window_; }
  UINT message_id() const { return message_id_; }

  // Overridden from StatusIcon:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const std::u16string& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override;
  void ForceVisible() override;

 protected:
  // Overridden from StatusIcon:
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override;

 private:
  void InitIconData(NOTIFYICONDATA* icon_data);

  // The tray that owns us.  Weak.
  raw_ptr<StatusTrayWin> tray_;

  // The unique ID corresponding to this icon.
  UINT icon_id_;

  // Window used for processing messages from this icon.
  HWND window_;

  // The message identifier used for status icon messages.
  UINT message_id_;

  // The currently-displayed icon for the window.
  base::win::ScopedHICON icon_;

  // The currently-displayed icon for the notification balloon.
  base::win::ScopedHICON balloon_icon_;

  // Not owned.
  raw_ptr<ui::MenuModel> menu_model_ = nullptr;

  // Context menu associated with this icon (if any).
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
