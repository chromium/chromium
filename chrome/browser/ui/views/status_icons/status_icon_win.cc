// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_win.h"

#include <string.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/ui/views/status_icons/status_tray_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/win/icon_util.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

using RefCountedIcon = base::RefCountedData<base::win::ScopedGDIObject<HICON>>;

StatusIconWin::ShellNotifyIconCallback& GetShellNotifyIconCallback() {
  static base::NoDestructor<StatusIconWin::ShellNotifyIconCallback> callback;
  return *callback;
}

// Only the owned request and its resources are accessed on the worker.
// In particular, this must not depend on UI objects or shutdown-time globals:
// Shell_NotifyIcon can outlive ThreadPool shutdown if Explorer is unresponsive.
void NotifyIconOnBackgroundThread(
    scoped_refptr<StatusTrayWin::Window> window,
    StatusIconWin::ShellNotifyIconCallback callback,
    DWORD message,
    std::unique_ptr<NOTIFYICONDATA> data,
    scoped_refptr<RefCountedIcon> icon,
    scoped_refptr<RefCountedIcon> balloon_icon) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (callback) {
    callback.Run(message, *data);
  } else {
    Shell_NotifyIcon(message, data.get());
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// StatusIconWin, public:

StatusIconWin::StatusIconWin(base::WeakPtr<StatusTrayWin> tray,
                             UINT id,
                             scoped_refptr<StatusTrayWin::Window> window,
                             UINT message)
    : tray_(std::move(tray)),
      icon_id_(id),
      window_(std::move(window)),
      message_id_(message) {
  auto icon_data = InitIconData();
  icon_data->uFlags = NIF_MESSAGE;
  icon_data->uCallbackMessage = message_id_;
  NotifyIcon(NIM_ADD, std::move(icon_data));
}

StatusIconWin::~StatusIconWin() {
  // Remove our icon.
  NotifyIcon(NIM_DELETE, InitIconData());
}

void StatusIconWin::HandleClickEvent(const gfx::Point& cursor_pos,
                                     bool left_mouse_click) {
  // Pass to the observer if appropriate.
  if (left_mouse_click && HasObservers()) {
    DispatchClickEvent();
    return;
  }

  if (!menu_model_) {
    return;
  }

  // Set our window as the foreground window, so the context menu closes when
  // we click away from it.
  if (!SetForegroundWindow(window())) {
    return;
  }

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_, views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(nullptr, nullptr, gfx::Rect(cursor_pos, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kMouse);
}

void StatusIconWin::HandleBalloonClickEvent() {
  if (HasObservers()) {
    DispatchBalloonClickEvent();
  }
}

void StatusIconWin::ResetIcon() {
  // Delete any previously existing icon.
  NotifyIcon(NIM_DELETE, InitIconData());

  auto icon_data = InitIconData();
  icon_data->uFlags = NIF_MESSAGE;
  icon_data->uCallbackMessage = message_id_;
  icon_data->hIcon = icon_ ? icon_->data.get() : nullptr;
  // If we have an image, then set the NIF_ICON flag, which tells
  // Shell_NotifyIcon() to set the image for the status icon it creates.
  if (icon_data->hIcon) {
    icon_data->uFlags |= NIF_ICON;
  }
  // Re-add our icon.
  NotifyIcon(NIM_ADD, std::move(icon_data));
}

void StatusIconWin::SetImage(const gfx::ImageSkia& image) {
  // Create the icon.
  auto icon_data = InitIconData();
  icon_data->uFlags = NIF_ICON;
  icon_ = base::MakeRefCounted<Icon>(
      IconUtil::CreateHICONFromSkBitmap(*image.bitmap()));
  icon_data->hIcon = icon_->data.get();
  NotifyIcon(NIM_MODIFY, std::move(icon_data));
}

void StatusIconWin::SetToolTip(const std::u16string& tool_tip) {
  // Create the icon.
  auto icon_data = InitIconData();
  icon_data->uFlags = NIF_TIP;
  base::wcslcpy(icon_data->szTip, base::as_wcstr(tool_tip));
  NotifyIcon(NIM_MODIFY, std::move(icon_data));
}

void StatusIconWin::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  auto icon_data = InitIconData();
  icon_data->uFlags = NIF_INFO;
  icon_data->dwInfoFlags = NIIF_INFO;
  base::wcslcpy(icon_data->szInfoTitle, base::as_wcstr(title));
  base::wcslcpy(icon_data->szInfo, base::as_wcstr(contents));
  icon_data->uTimeout = 0;

  if (!icon.isNull()) {
    balloon_icon_ = base::MakeRefCounted<Icon>(
        IconUtil::CreateHICONFromSkBitmap(*icon.bitmap()));
    icon_data->hBalloonIcon = balloon_icon_->data.get();
    icon_data->dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
  }

  NotifyIcon(NIM_MODIFY, std::move(icon_data));
}

void StatusIconWin::ForceVisible() {
  // Registration is asynchronous. Do not start the COM visibility request
  // until preceding Shell requests have completed.
  window_->task_runner()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&StatusIconWin::UpdateIconVisibility,
                     weak_factory_.GetWeakPtr()));
}

void StatusIconWin::UpdateIconVisibility() {
  if (tray_) {
    tray_->UpdateIconVisibilityInBackground(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// StatusIconWin, private:

void StatusIconWin::UpdatePlatformContextMenu(StatusIconMenuModel* menu) {
  // |menu_model_| is about to be destroyed. Destroy the menu (which closes it)
  // so that it doesn't attempt to continue using |menu_model_|.
  menu_runner_.reset();
  CHECK(menu, base::NotFatalUntil::M162);
  menu_model_ = menu;
}

std::unique_ptr<NOTIFYICONDATA> StatusIconWin::InitIconData() const {
  auto data = std::make_unique<NOTIFYICONDATA>();
  data->cbSize = sizeof(NOTIFYICONDATA);
  data->hWnd = window();
  data->uID = icon_id_;
  return data;
}

// static
void StatusIconWin::SetShellNotifyIconForTesting(
    ShellNotifyIconCallback callback) {
  GetShellNotifyIconCallback() = std::move(callback);
}

void StatusIconWin::NotifyIcon(DWORD message,
                               std::unique_ptr<NOTIFYICONDATA> data) {
  // Owning NOTIFYICONDATA alone does not retain either icon handle. Share
  // ownership of the matching image resources with the request instead of
  // borrowing handles that SetImage/DisplayBalloon or destruction can release.
  auto icon = data->hIcon ? icon_ : nullptr;
  auto balloon_icon = data->hBalloonIcon ? balloon_icon_ : nullptr;
  window_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NotifyIconOnBackgroundThread, window_,
                     GetShellNotifyIconCallback(), message, std::move(data),
                     std::move(icon), std::move(balloon_icon)));
}
