// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_win.h"

#include <string.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/status_icons/status_tray_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/controls/menu/menu_runner.h"

////////////////////////////////////////////////////////////////////////////////
// StatusIconWin, public:

StatusIconWin::StatusIconWin(StatusTrayWin* tray,
                             UINT id,
                             HWND window,
                             UINT message)
    : tray_(tray), icon_id_(id), window_(window), message_id_(message) {
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_MESSAGE;
  icon_data.uCallbackMessage = message_id_;
  BOOL result = Shell_NotifyIcon(NIM_ADD, &icon_data);
  // This can happen if the explorer process isn't running when we try to
  // create the icon for some reason (for example, at startup).
  if (!result)
    LOG(WARNING) << "Unable to create status tray icon.";
}

StatusIconWin::~StatusIconWin() {
  // Remove our icon.
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  Shell_NotifyIcon(NIM_DELETE, &icon_data);
}

void StatusIconWin::HandleClickEvent(const gfx::Point& cursor_pos,
                                     bool left_mouse_click) {
  // Pass to the observer if appropriate.
  if (left_mouse_click && HasObservers()) {
    DispatchClickEvent();
    return;
  }

  if (!menu_model_)
    return;

  // Set our window as the foreground window, so the context menu closes when
  // we click away from it.
  if (!SetForegroundWindow(window_))
    return;

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_, views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(nullptr, nullptr, gfx::Rect(cursor_pos, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_MOUSE);
}

void StatusIconWin::HandleBalloonClickEvent() {
  if (HasObservers())
    DispatchBalloonClickEvent();
}

void StatusIconWin::ResetIcon() {
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  // Delete any previously existing icon.
  Shell_NotifyIcon(NIM_DELETE, &icon_data);
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_MESSAGE;
  icon_data.uCallbackMessage = message_id_;
  icon_data.hIcon = icon_.get();
  // If we have an image, then set the NIF_ICON flag, which tells
  // Shell_NotifyIcon() to set the image for the status icon it creates.
  if (icon_data.hIcon)
    icon_data.uFlags |= NIF_ICON;
  // Re-add our icon.
  BOOL result = Shell_NotifyIcon(NIM_ADD, &icon_data);
  if (!result)
    LOG(WARNING) << "Unable to re-create status tray icon.";
}

void StatusIconWin::SetImage(const gfx::ImageSkia& image) {
  // Create the icon.
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_ICON;
  icon_ = IconUtil::CreateHICONFromSkBitmap(*image.bitmap());
  icon_data.hIcon = icon_.get();
  BOOL result = Shell_NotifyIcon(NIM_MODIFY, &icon_data);
  if (!result)
    LOG(WARNING) << "Error setting status tray icon image";
}

void StatusIconWin::SetToolTip(const std::u16string& tool_tip) {
  // Create the icon.
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_TIP;
  wcscpy_s(icon_data.szTip, base::as_wcstr(tool_tip));
  BOOL result = Shell_NotifyIcon(NIM_MODIFY, &icon_data);
  if (!result)
    LOG(WARNING) << "Unable to set tooltip for status tray icon";
}

void StatusIconWin::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_INFO;
  icon_data.dwInfoFlags = NIIF_INFO;
  wcscpy_s(icon_data.szInfoTitle, base::as_wcstr(title));
  wcscpy_s(icon_data.szInfo, base::as_wcstr(contents));
  icon_data.uTimeout = 0;

  if (!icon.isNull()) {
    balloon_icon_ = IconUtil::CreateHICONFromSkBitmap(*icon.bitmap());
    icon_data.hBalloonIcon = balloon_icon_.get();
    icon_data.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
  }

  BOOL result = Shell_NotifyIcon(NIM_MODIFY, &icon_data);
  if (!result)
    LOG(WARNING) << "Unable to create status tray balloon.";
}

void StatusIconWin::ForceVisible() {
  tray_->UpdateIconVisibilityInBackground(this);
}

////////////////////////////////////////////////////////////////////////////////
// StatusIconWin, private:

void StatusIconWin::UpdatePlatformContextMenu(StatusIconMenuModel* menu) {
  // |menu_model_| is about to be destroyed. Destroy the menu (which closes it)
  // so that it doesn't attempt to continue using |menu_model_|.
  menu_runner_.reset();
  DCHECK(menu);
  menu_model_ = menu;
}

void StatusIconWin::InitIconData(NOTIFYICONDATA* icon_data) {
  memset(icon_data, 0, sizeof(NOTIFYICONDATA));
  icon_data->cbSize = sizeof(NOTIFYICONDATA);

  icon_data->hWnd = window_;
  icon_data->uID = icon_id_;
}
