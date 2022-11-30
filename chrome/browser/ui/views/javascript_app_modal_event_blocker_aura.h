// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_AURA_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"

class BrowserView;

namespace ui {
class EventTarget;
}

// JavascriptAppModalEventBlockerAura blocks events to all browser windows
// except the browser window which hosts |app_modal_window| for the duration of
// its lifetime. JavascriptAppModalEventBlockerAura should not outlive
// |app_modal_window|.
// TODO(pkotwicz): Merge this class into WindowModalityController.
class JavascriptAppModalEventBlockerAura : public ui::EventHandler {
 public:
  explicit JavascriptAppModalEventBlockerAura(aura::Window* app_modal_window);
  JavascriptAppModalEventBlockerAura(
      const JavascriptAppModalEventBlockerAura&) = delete;
  JavascriptAppModalEventBlockerAura& operator=(
      const JavascriptAppModalEventBlockerAura&) = delete;
  ~JavascriptAppModalEventBlockerAura() override;

 private:
  // Returns true if the propagation of events to |target| should be stopped.
  bool ShouldStopPropagationTo(ui::EventTarget* target);

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // The app modal dialog.
  raw_ptr<aura::Window> modal_window_;

  // The BrowserView which hosts the app modal dialog.
  raw_ptr<BrowserView> browser_view_with_modal_dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_AURA_H_
