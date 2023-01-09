// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_WIDGET_FOCUS_WAITER_H_
#define CHROME_TEST_INTERACTION_WIDGET_FOCUS_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget.h"

// Performs an action and then waits for the given widget to become focused.
// Because it is hard to determine if a widget is *already* focused, we instead
// assume that the widget is not focused, and place the action that should
// cause the widget to become focused in the event queue before doing the wait.
//
// TODO(dfried): consider moving this to ui/views; it's quite useful.
class WidgetFocusWaiter : public views::WidgetFocusChangeListener {
 public:
  explicit WidgetFocusWaiter(views::Widget* widget);
  WidgetFocusWaiter(const WidgetFocusWaiter& other) = delete;
  ~WidgetFocusWaiter() override;
  void operator=(const WidgetFocusWaiter& other) = delete;

  // Performs `action` and then waits for the target widget to become focused.
  // The action should cause the widget to become focused, or the test will
  // time out.
  void WaitAfter(base::OnceClosure action);

 private:
  // views::WidgetFocusChangeListener
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  base::RunLoop run_loop_;
  base::OnceClosure closure_;
  const raw_ptr<views::Widget> widget_;
};

#endif  // CHROME_TEST_INTERACTION_WIDGET_FOCUS_WAITER_H_
