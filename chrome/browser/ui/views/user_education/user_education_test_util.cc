// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/user_education_test_util.h"

#include "base/threading/thread_task_runner_handle.h"

WidgetFocusWaiter::WidgetFocusWaiter(views::Widget* widget) : widget_(widget) {
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

WidgetFocusWaiter::~WidgetFocusWaiter() {
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void WidgetFocusWaiter::WaitAfter(base::OnceClosure action) {
  CHECK(closure_.is_null());
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(action));
  closure_ = run_loop_.QuitClosure();
  run_loop_.Run();
}

void WidgetFocusWaiter::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (!closure_.is_null() && widget_->GetNativeView() == focused_now)
    std::move(closure_).Run();
}
