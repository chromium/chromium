// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/window_util.h"

namespace {

aura::Window* GetPopupAuraWindow(aura::Window* current) {
  DCHECK(current);
  while (current && (current->GetType() != aura::client::WINDOW_TYPE_POPUP))
    current = current->parent();
  return current;
}

class AuraWindowObserver : public aura::WindowObserver {
 public:
  AuraWindowObserver(const aura::Window* popup_window, base::RunLoop* run_loop)
      : popup_window_(popup_window), run_loop_(run_loop) {}
  AuraWindowObserver(const AuraWindowObserver&) = delete;
  AuraWindowObserver& operator=(const AuraWindowObserver&) = delete;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (popup_window_ == window && visible)
      run_loop_->QuitWhenIdle();
  }

 private:
  const raw_ptr<const aura::Window> popup_window_;
  const raw_ptr<base::RunLoop> run_loop_;
};

}  // namespace

void ExtensionActionTestHelper::WaitForPopup() {
  // The popup starts out active but invisible, so all we need to really do is
  // look for visibility.
  aura::Window* native_view = GetPopupNativeView();
  ASSERT_TRUE(native_view);

  aura::Window* popup = GetPopupAuraWindow(native_view);
  ASSERT_TRUE(popup);

  if (!popup->IsVisible()) {
    base::RunLoop run_loop;
    AuraWindowObserver observer(popup, &run_loop);
    popup->AddObserver(&observer);
    run_loop.Run();
    DCHECK(wm::IsActiveWindow(popup));
    popup->RemoveObserver(&observer);
  }

  ASSERT_TRUE(popup->IsVisible());
  ASSERT_TRUE(HasPopup());
}
