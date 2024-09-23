// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/javascript_app_modal_event_blocker.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace {

// Returns the toplevel window for the deepest transient ancestor of |window|.
aura::Window* GetTopmostTransientParent(aura::Window* window) {
  aura::Window* topmost = wm::GetToplevelWindow(window);
  while (topmost && wm::GetTransientParent(topmost))
    topmost = wm::GetToplevelWindow(wm::GetTransientParent(topmost));
  return topmost;
}

}  // namespace

JavascriptAppModalEventBlockerAura::JavascriptAppModalEventBlockerAura(
    aura::Window* modal_window)
    : modal_window_(modal_window), browser_view_with_modal_dialog_(nullptr) {
  aura::Window* topmost_transient_parent =
      GetTopmostTransientParent(modal_window);
  browser_view_with_modal_dialog_ =
      BrowserView::GetBrowserViewForNativeWindow(topmost_transient_parent);
  // |browser_view_with_modal_dialog_| is nullptr if the dialog was opened by an
  // extension background page.

  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);

  // WindowModalityController will cancel touches as appropriate.
}

JavascriptAppModalEventBlockerAura::~JavascriptAppModalEventBlockerAura() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

bool JavascriptAppModalEventBlockerAura::ShouldStopPropagationTo(
    ui::EventTarget* target) {
  // Stop propagation if:
  // -|target| is a browser window or a transient child of a browser window.
  // -|target| is not the browser window which hosts |modal_window_| and not
  //  a transient child of that browser window.
  // WindowModalityController will stop the transient parent from handling the
  // event if the user clicks the modal window's transient parent (as opposed to
  // clicking the modal window itself).
  aura::Window* window =
      GetTopmostTransientParent(static_cast<aura::Window*>(target));
  if (!window)
    return false;
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view && browser_view != browser_view_with_modal_dialog_;
}

void JavascriptAppModalEventBlockerAura::OnKeyEvent(ui::KeyEvent* event) {
  if (ShouldStopPropagationTo(event->target()))
    event->StopPropagation();
}

void JavascriptAppModalEventBlockerAura::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMouseCaptureChanged &&
      ShouldStopPropagationTo(event->target())) {
    if (event->type() == ui::EventType::kMousePressed) {
      wm::AnimateWindow(modal_window_, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    }
    event->StopPropagation();
  }
}

void JavascriptAppModalEventBlockerAura::OnScrollEvent(ui::ScrollEvent* event) {
  if (ShouldStopPropagationTo(event->target()))
    event->StopPropagation();
}

void JavascriptAppModalEventBlockerAura::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::EventType::kTouchCancelled &&
      ShouldStopPropagationTo(event->target())) {
    if (event->type() == ui::EventType::kTouchPressed) {
      wm::AnimateWindow(modal_window_, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    }
    event->StopPropagation();
  }
}
