// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_test_util.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/wm/core/window_util.h"

namespace test {
namespace {
// Wait until the window's state changes to given the snapped state.
// The window should stay alive, so no need to observer destroying.
class SnapWaiter : public aura::WindowObserver {
 public:
  SnapWaiter(aura::Window* window, ash::WindowStateType type)
      : window_(window), type_(type) {
    window->AddObserver(this);
  }
  ~SnapWaiter() override { window_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == ash::kWindowStateTypeKey && IsSnapped())
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  bool IsSnapped() const {
    return window_->GetProperty(ash::kWindowStateTypeKey) == type_;
  }

 private:
  aura::Window* window_;
  ash::WindowStateType type_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SnapWaiter);
};

}  // namespace

void ActivateAndSnapWindow(aura::Window* window, ash::WindowStateType type) {
  DCHECK(window);
  if (!wm::IsActiveWindow(window))
    wm::ActivateWindow(window);

  ASSERT_TRUE(wm::IsActiveWindow(window));

  SnapWaiter snap_waiter(window, type);
  ASSERT_TRUE(type == ash::WindowStateType::kRightSnapped ||
              type == ash::WindowStateType::kLeftSnapped);

  // Early return if it's already snapped.
  if (snap_waiter.IsSnapped())
    return;

  ui_controls::SendKeyPress(window,
                            type == ash::WindowStateType::kLeftSnapped
                                ? ui::VKEY_OEM_4
                                : ui::VKEY_OEM_6,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/true,
                            /*command=*/false);
  snap_waiter.Wait();
}

}  // namespace test
