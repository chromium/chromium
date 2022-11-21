// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/widget_focus_waiter.h"

WidgetFocusWaiter::WidgetFocusWaiter(views::Widget* widget) : widget_(widget) {
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

WidgetFocusWaiter::~WidgetFocusWaiter() {
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void WidgetFocusWaiter::WaitAfter(base::OnceClosure action) {
  CHECK(closure_.is_null());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(action));
  closure_ = run_loop_.QuitClosure();
  run_loop_.Run();
}

void WidgetFocusWaiter::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (!closure_.is_null() && widget_->GetNativeView() == focused_now)
    std::move(closure_).Run();
}
