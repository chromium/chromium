// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_

#include <windows.h>

#include <shellapi.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

namespace gfx {
class Point;
}

namespace views {
class MenuRunner;
}

class StatusIconWin : public StatusIcon {
 public:
  // Constructor which provides this icon's unique ID and messaging window.
  StatusIconWin(base::WeakPtr<StatusTrayWin> tray,
                UINT id,
                scoped_refptr<StatusTrayWin::Window> window,
                UINT message);

  using ShellNotifyIconCallback =
      base::RepeatingCallback<BOOL(DWORD, const NOTIFYICONDATA&)>;
  // Set/reset on the UI sequence, with no outstanding requests. Each request
  // owns a copy of the callback, just as it owns its notification data.
  static void SetShellNotifyIconForTesting(ShellNotifyIconCallback callback);

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
  HWND window() const { return window_->hwnd(); }
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
  std::unique_ptr<NOTIFYICONDATA> InitIconData() const;
  void NotifyIcon(DWORD message, std::unique_ptr<NOTIFYICONDATA> data);
  void UpdateIconVisibility();

  // An icon removed from a tray may outlive the tray.
  base::WeakPtr<StatusTrayWin> tray_;

  // The unique ID corresponding to this icon.
  UINT icon_id_;

  // Window used for processing messages from this icon.
  scoped_refptr<StatusTrayWin::Window> window_;

  // The message identifier used for status icon messages.
  UINT message_id_;

  // Requests retain these resources independently of subsequent updates and
  // destruction of this object.
  using Icon = base::RefCountedData<base::win::ScopedGDIObject<HICON>>;
  scoped_refptr<Icon> icon_;
  scoped_refptr<Icon> balloon_icon_;

  // Not owned.
  raw_ptr<ui::MenuModel> menu_model_ = nullptr;

  // Context menu associated with this icon (if any).
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::WeakPtrFactory<StatusIconWin> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
