// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <shellapi.h>
#include <stdint.h>
#include <windowsx.h>
#include <wrl/client.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "chrome/browser/ui/views/status_icons/status_tray_state_changer_win.h"
#include "chrome/common/chrome_constants.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/window_impl.h"

static const UINT kStatusIconMessage = WM_APP + 1;

namespace {
// |kBaseIconId| is 2 to avoid conflicts with plugins that hard-code id 1.
const UINT kBaseIconId = 2;

UINT ReservedIconId(StatusTray::StatusIconType type) {
  return kBaseIconId + static_cast<UINT>(type);
}

gfx::Point AnchorPointFromWParam(WPARAM wparam) {
  gfx::PointF anchor_pos(GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam));
  if (auto* screen_win = display::win::GetScreenWin()) {
    anchor_pos = screen_win->ScreenToDIPPoint(anchor_pos);
  }
  return gfx::ToFlooredPoint(anchor_pos);
}

}  // namespace

// Default implementation for StatusTrayStateChanger that communicates to
// Exporer.exe via COM.  It spawns a background thread with a fresh COM
// apartment and requests that the visibility be increased unless the user
// has explicitly set the icon to be hidden.
class StatusTrayStateChangerProxyImpl : public StatusTrayStateChangerProxy {
 public:
  StatusTrayStateChangerProxyImpl()
      : pending_requests_(0), worker_thread_("StatusIconCOMWorkerThread") {
    worker_thread_.init_com_with_mta(false);
  }

  StatusTrayStateChangerProxyImpl(const StatusTrayStateChangerProxyImpl&) =
      delete;
  StatusTrayStateChangerProxyImpl& operator=(
      const StatusTrayStateChangerProxyImpl&) = delete;

  ~StatusTrayStateChangerProxyImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void EnqueueChange(UINT icon_id, HWND window) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_requests_ == 0) {
      worker_thread_.Start();
    }

    ++pending_requests_;
    worker_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &StatusTrayStateChangerProxyImpl::EnqueueChangeOnWorkerThread,
            icon_id, window),
        base::BindOnce(&StatusTrayStateChangerProxyImpl::ChangeDone,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  // Must be called only on |worker_thread_|, to ensure the correct COM
  // apartment.
  static void EnqueueChangeOnWorkerThread(UINT icon_id, HWND window) {
    Microsoft::WRL::ComPtr<StatusTrayStateChangerWin>
        status_tray_state_changer =
            Microsoft::WRL::Make<StatusTrayStateChangerWin>(icon_id, window);
    status_tray_state_changer->EnsureTrayIconVisible();
  }

  // Called on UI thread.
  void ChangeDone() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_GT(pending_requests_, 0, base::NotFatalUntil::M162);

    if (--pending_requests_ == 0) {
      worker_thread_.Stop();
    }
  }

 private:
  int pending_requests_;
  base::Thread worker_thread_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StatusTrayStateChangerProxyImpl> weak_factory_{this};
};

// WindowImpl shares its registered window class across instances, including
// when a new tray is created before an old tray's Shell requests finish.
class StatusTrayWin::Window::MessageWindow : public gfx::WindowImpl {
 public:
  explicit MessageWindow(StatusTrayWin* tray)
      : WindowImpl("StatusTrayWin"), tray_(tray) {
    set_window_class_name(chrome::kStatusTrayWindowClass);
    set_initial_class_style(0);
    // Message-only windows do not receive the TaskbarCreated broadcast.
    set_window_style(WS_POPUP);
    Init(nullptr, gfx::Rect());
  }

  ~MessageWindow() override {
    ClearUserData();
    DestroyWindow(hwnd());
  }

  void Detach() { tray_ = nullptr; }

 private:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM wparam,
                            LPARAM lparam,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    if (!tray_) {
      return FALSE;
    }
    result = tray_->WndProc(window, message, wparam, lparam);
    return TRUE;
  }

  raw_ptr<StatusTrayWin> tray_;
};

StatusTrayWin::Window::Window(StatusTrayWin* tray)
    : base::RefCountedDeleteOnSequence<Window>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      message_window_(std::make_unique<MessageWindow>(tray)),
      hwnd_(message_window_->hwnd()),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           // Do not let an unresponsive Shell block browser shutdown. Requests
           // that have not started are skipped, including icon removal. A
           // skipped removal may leave a stale tray icon until the Shell
           // refreshes it. Running requests retain their resources but are not
           // waited on during shutdown.
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

StatusTrayWin::Window::~Window() {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence(),
        base::NotFatalUntil::M163);
}

void StatusTrayWin::Window::Detach() {
  CHECK(owning_task_runner()->RunsTasksInCurrentSequence(),
        base::NotFatalUntil::M163);
  message_window_->Detach();
}

StatusTrayWin::StatusTrayWin()
    : next_icon_id_(0),
      taskbar_created_message_(RegisterWindowMessage(L"TaskbarCreated")) {
  window_ = base::MakeRefCounted<Window>(this);
}

StatusTrayWin::~StatusTrayWin() {
  // Requests retain the HWND, but must no longer dispatch to this object.
  // The last request releases the window back to its creating UI sequence.
  window_->Detach();
}

void StatusTrayWin::UpdateIconVisibilityInBackground(
    StatusIconWin* status_icon) {
  if (!state_changer_proxy_.get()) {
    state_changer_proxy_ = std::make_unique<StatusTrayStateChangerProxyImpl>();
  }

  state_changer_proxy_->EnqueueChange(status_icon->icon_id(),
                                      status_icon->window());
}

LRESULT CALLBACK StatusTrayWin::WndProc(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  if (message == taskbar_created_message_) {
    pending_mouse_context_menu_icon_id_.reset();
    // We need to reset all of our icons because the taskbar went away.
    for (const auto& i : status_icons()) {
      auto* win_icon = static_cast<StatusIconWin*>(i.icon.get());
      win_icon->ResetIcon();
    }
    return TRUE;
  } else if (message == kStatusIconMessage) {
    StatusIconWin* win_icon = nullptr;
    // Under NOTIFYICON_VERSION_4, HIWORD(lparam) carries the 16-bit icon ID,
    // LOWORD(lparam) carries the notification event, and wparam carries the
    // anchor screen coordinates.
    const UINT icon_id = HIWORD(lparam);
    const UINT event = LOWORD(lparam);

    // Find the selected status icon.
    for (const auto& i : status_icons()) {
      auto* current_win_icon = static_cast<StatusIconWin*>(i.icon.get());
      if (current_win_icon->icon_id() == icon_id) {
        win_icon = current_win_icon;
        break;
      }
    }

    // It is possible for this procedure to be called with an obsolete icon
    // id.  In that case we should just return early before handling any
    // actions.
    if (!win_icon) {
      return TRUE;
    }

    switch (event) {
      case NIN_BALLOONUSERCLICK:
        pending_mouse_context_menu_icon_id_.reset();
        win_icon->HandleBalloonClickEvent();
        return TRUE;

      case WM_RBUTTONUP:
        pending_mouse_context_menu_icon_id_ = icon_id;
        return TRUE;

      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
        pending_mouse_context_menu_icon_id_.reset();
        return TRUE;

      case NIN_SELECT:
      case NIN_KEYSELECT:
        pending_mouse_context_menu_icon_id_.reset();
        win_icon->HandleActivation(AnchorPointFromWParam(wparam),
                                   /*is_primary_activation=*/true,
                                   event == NIN_KEYSELECT
                                       ? ui::mojom::MenuSourceType::kKeyboard
                                       : ui::mojom::MenuSourceType::kMouse);
        return TRUE;

      case WM_CONTEXTMENU: {
        const bool from_mouse = pending_mouse_context_menu_icon_id_ == icon_id;
        pending_mouse_context_menu_icon_id_.reset();
        win_icon->HandleActivation(AnchorPointFromWParam(wparam),
                                   /*is_primary_activation=*/false,
                                   from_mouse
                                       ? ui::mojom::MenuSourceType::kMouse
                                       : ui::mojom::MenuSourceType::kKeyboard);
        return TRUE;
      }
    }
  } else if (message == WM_ENDSESSION) {
    // If Chrome is in background-only mode, this is the only notification
    // it gets that Windows is exiting. Make sure we shutdown in an orderly
    // fashion.
    chrome::SessionEnding();
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

std::unique_ptr<StatusIcon> StatusTrayWin::CreatePlatformStatusIcon(
    StatusTray::StatusIconType type,
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip) {
  UINT next_icon_id;
  if (type == StatusTray::OTHER_ICON) {
    next_icon_id = NextIconId();
  } else {
    next_icon_id = ReservedIconId(type);
  }

  auto icon = std::make_unique<StatusIconWin>(
      weak_factory_.GetWeakPtr(), next_icon_id, window_, kStatusIconMessage);

  icon->SetImage(image);
  icon->SetToolTip(tool_tip);
  return std::move(icon);
}

UINT StatusTrayWin::NextIconId() {
  constexpr UINT kFirstDynamicIconId =
      kBaseIconId + static_cast<UINT>(NAMED_STATUS_ICON_COUNT) + 1;
  constexpr UINT kDynamicIconIdSpan = UINT16_MAX - kFirstDynamicIconId + 1;
  const UINT offset = next_icon_id_++ % kDynamicIconIdSpan;
  return kFirstDynamicIconId + offset;
}

void StatusTrayWin::SetStatusTrayStateChangerProxyForTest(
    std::unique_ptr<StatusTrayStateChangerProxy> proxy) {
  state_changer_proxy_ = std::move(proxy);
}

std::unique_ptr<StatusTray> StatusTray::Create() {
  return std::make_unique<StatusTrayWin>();
}
