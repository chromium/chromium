// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_aura.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromecast {

namespace {

CastGestureHandler::Priority ToGestureHandlerPriority(
    mojom::GesturePriority priority) {
  switch (priority) {
    case mojom::GesturePriority::NONE:
      return CastGestureHandler::Priority::NONE;
    case mojom::GesturePriority::ROOT_UI:
      return CastGestureHandler::Priority::ROOT_UI;
    case mojom::GesturePriority::MAIN_ACTIVITY:
      return CastGestureHandler::Priority::MAIN_ACTIVITY;
    case mojom::GesturePriority::SETTINGS_UI:
      return CastGestureHandler::Priority::SETTINGS_UI;
  }
}

}  // namespace
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

  TouchBlocker(const TouchBlocker&) = delete;
  TouchBlocker& operator=(const TouchBlocker&) = delete;

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
};

CastContentWindowAura::CastContentWindowAura(mojom::CastWebViewParamsPtr params,
                                             CastWindowManager* window_manager)
    : CastContentWindow(std::move(params)),
      window_manager_(window_manager),
      gesture_dispatcher_(
          std::make_unique<CastContentGestureHandler>(gesture_router())),
      window_(nullptr),
      has_screen_access_(false),
      resize_window_when_navigation_starts_(true) {}

CastContentWindowAura::~CastContentWindowAura() {
  content::WebContentsObserver::Observe(nullptr);
  CastWebContentsObserver::Observe(nullptr);
  if (window_manager_) {
    window_manager_->RemoveGestureHandler(gesture_dispatcher_.get());
  }
  if (window_) {
    window_->RemoveObserver(this);
  }
}

void CastContentWindowAura::CreateWindow(
    mojom::ZOrder z_order,
    VisibilityPriority visibility_priority) {
  DCHECK(window_manager_) << "A CastWindowManager must be provided before "
                          << "creating a window for WebContents.";
  CastWebContentsObserver::Observe(cast_web_contents());
  content::WebContentsObserver::Observe(WebContents());
  window_ = WebContents()->GetNativeView();
  if (!window_->HasObserver(this)) {
    window_->AddObserver(this);
  }
  window_manager_->SetZOrder(window_, z_order);
  window_manager_->AddWindow(window_);
  window_manager_->AddGestureHandler(gesture_dispatcher_.get());

  touch_blocker_ =
      std::make_unique<TouchBlocker>(window_, !params_->enable_touch_input);

  if (has_screen_access_) {
    window_->Show();
  } else {
    window_->Hide();
  }

  cast_web_contents()->web_contents()->Focus();
}

void CastContentWindowAura::GrantScreenAccess() {
  has_screen_access_ = true;
  if (window_) {
    SetFullWindowBounds();
    window_->Show();
  }
}

void CastContentWindowAura::RevokeScreenAccess() {
  has_screen_access_ = false;
  resize_window_when_navigation_starts_ = false;
  if (window_) {
    window_->Hide();
    SetHiddenWindowBounds();
  }
}

void CastContentWindowAura::EnableTouchInput(bool enabled) {
  if (touch_blocker_) {
    touch_blocker_->Activate(!enabled);
  }
}

void CastContentWindowAura::RequestVisibility(
    VisibilityPriority visibility_priority) {}

void CastContentWindowAura::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (visible) {
    gesture_dispatcher_->SetPriority(
        ToGestureHandlerPriority(params_->gesture_priority));
  } else {
    gesture_dispatcher_->SetPriority(CastGestureHandler::Priority::NONE);
  }
}

void CastContentWindowAura::OnWindowDestroyed(aura::Window* window) {
  window_ = nullptr;
}

void CastContentWindowAura::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!resize_window_when_navigation_starts_ || !window_) {
    return;
  }
  resize_window_when_navigation_starts_ = false;
  SetFullWindowBounds();
}

void CastContentWindowAura::SetFullWindowBounds() {
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  window_->SetBounds(gfx::Rect(display_size.width(), display_size.height()));
#endif
}

void CastContentWindowAura::SetHiddenWindowBounds() {
  // Because rendering a larger window may require more system resources,
  // resize the window to one pixel while hidden.
  LOG(INFO) << "Resizing window to 1x1 pixel while hidden";
  window_->SetBounds(gfx::Rect(1, 1));
}

}  // namespace chromecast
