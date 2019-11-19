// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_aura.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/ui/media_control_ui.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromecast {

class TouchBlocker : public ui::EventHandler, public aura::WindowObserver {
 public:
  TouchBlocker(aura::Window* window, bool activated)
      : window_(window), activated_(activated) {
    DCHECK(window_);
    window_->AddObserver(this);
    if (activated_) {
      window_->AddPreTargetHandler(this);
    }
  }

  ~TouchBlocker() override {
    if (window_) {
      window_->RemoveObserver(this);
      if (activated_) {
        window_->RemovePreTargetHandler(this);
      }
    }
  }

  void Activate(bool activate) {
    if (!window_ || activate == activated_) {
      return;
    }

    if (activate) {
      window_->AddPreTargetHandler(this);
    } else {
      window_->RemovePreTargetHandler(this);
    }

    activated_ = activate;
  }

 private:
  // Overriden from ui::EventHandler.
  void OnTouchEvent(ui::TouchEvent* touch) override {
    if (activated_) {
      touch->SetHandled();
    }
  }

  // Overriden from aura::WindowObserver.
  void OnWindowDestroyed(aura::Window* window) override { window_ = nullptr; }

  aura::Window* window_;
  bool activated_;

  DISALLOW_COPY_AND_ASSIGN(TouchBlocker);
};

CastContentWindowAura::CastContentWindowAura(
    const CastContentWindow::CreateParams& params,
    CastWindowManager* window_manager)
    : CastContentWindow(params),
      window_manager_(window_manager),
      gesture_dispatcher_(
          std::make_unique<CastContentGestureHandler>(delegate_)),
      gesture_priority_(params.gesture_priority),
      is_touch_enabled_(params.enable_touch_input),
      window_(nullptr),
      has_screen_access_(false) {}

CastContentWindowAura::~CastContentWindowAura() {
  CastWebContents::Observer::Observe(nullptr);
  if (window_manager_) {
    window_manager_->RemoveGestureHandler(gesture_dispatcher_.get());
  }
  if (window_) {
    window_->RemoveObserver(this);
  }
}

void CastContentWindowAura::CreateWindowForWebContents(
    CastWebContents* cast_web_contents,
    mojom::ZOrder z_order,
    VisibilityPriority visibility_priority) {
  DCHECK(cast_web_contents);
  DCHECK(window_manager_) << "A CastWindowManager must be provided before "
                          << "creating a window for WebContents.";
  CastWebContents::Observer::Observe(cast_web_contents);
  window_ = cast_web_contents->web_contents()->GetNativeView();
  if (!window_->HasObserver(this)) {
    window_->AddObserver(this);
  }
  window_manager_->SetZOrder(window_, z_order);
  window_manager_->AddWindow(window_);
  window_manager_->AddGestureHandler(gesture_dispatcher_.get());

  touch_blocker_ = std::make_unique<TouchBlocker>(window_, !is_touch_enabled_);
  media_controls_ = std::make_unique<MediaControlUi>(window_manager_);

  if (has_screen_access_) {
    window_->Show();
  } else {
    window_->Hide();
  }
}

void CastContentWindowAura::GrantScreenAccess() {
  has_screen_access_ = true;
  if (window_) {
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
    gfx::Size display_size =
        display::Screen::GetScreen()->GetPrimaryDisplay().size();
    window_->SetBounds(gfx::Rect(display_size.width(), display_size.height()));
#endif
    window_->Show();
  }
}

void CastContentWindowAura::RevokeScreenAccess() {
  has_screen_access_ = false;
  if (window_) {
    window_->Hide();
    // Because rendering a larger window may require more system resources,
    // resize the window to one pixel while hidden.
    LOG(INFO) << "Resizing window to 1x1 pixel while hidden";
    window_->SetBounds(gfx::Rect(1, 1));
  }
}

void CastContentWindowAura::EnableTouchInput(bool enabled) {
  if (touch_blocker_) {
    touch_blocker_->Activate(!enabled);
  }
}

mojom::MediaControlUi* CastContentWindowAura::media_controls() {
  return media_controls_.get();
}

void CastContentWindowAura::MainFrameResized(const gfx::Rect& bounds) {
  if (media_controls_) {
    media_controls_->SetBounds(bounds);
  }
}

void CastContentWindowAura::RequestVisibility(
    VisibilityPriority visibility_priority) {}

void CastContentWindowAura::SetActivityContext(base::Value activity_context) {}

void CastContentWindowAura::SetHostContext(base::Value host_context) {}

void CastContentWindowAura::NotifyVisibilityChange(
    VisibilityType visibility_type) {
  if (delegate_) {
    delegate_->OnVisibilityChange(visibility_type);
  }
  for (auto& observer : observer_list_) {
    observer.OnVisibilityChange(visibility_type);
  }
}

void CastContentWindowAura::RequestMoveOut() {}

void CastContentWindowAura::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (visible) {
    gesture_dispatcher_->SetPriority(gesture_priority_);
  } else {
    gesture_dispatcher_->SetPriority(CastGestureHandler::Priority::NONE);
  }
}

void CastContentWindowAura::OnWindowDestroyed(aura::Window* window) {
  window_ = nullptr;
}

}  // namespace chromecast
