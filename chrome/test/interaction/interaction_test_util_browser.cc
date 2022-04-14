// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/views/interaction/interaction_test_util_views.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

std::unique_ptr<ui::test::InteractionTestUtil> CreateInteractionTestUtil() {
  auto test_util = std::make_unique<ui::test::InteractionTestUtil>();
  test_util->AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
#if BUILDFLAG(IS_MAC)
  test_util->AddSimulator(
      std::make_unique<ui::test::InteractionTestUtilSimulatorMac>());
#endif
  return test_util;
}

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
