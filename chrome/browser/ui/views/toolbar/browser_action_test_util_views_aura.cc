// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/browser_action_test_util.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/window_util.h"

namespace {

aura::Window* GetPopupAuraWindow(aura::Window* current) {
  DCHECK(current);
  while (current && (current->type() != aura::client::WINDOW_TYPE_POPUP))
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
  const aura::Window* const popup_window_;
  base::RunLoop* const run_loop_;
};

}  // namespace

bool BrowserActionTestUtil::WaitForPopup() {
  // The popup starts out active but invisible, so all we need to really do is
  // look for visibility.
  aura::Window* native_view = GetPopupNativeView();
  if (!native_view)
    return false;

  aura::Window* popup = GetPopupAuraWindow(native_view);
  if (!popup)
    return false;

  if (popup->IsVisible())
    return true;

  base::RunLoop run_loop;
  AuraWindowObserver observer(popup, &run_loop);
  popup->AddObserver(&observer);
  run_loop.Run();
  DCHECK(wm::IsActiveWindow(popup));
  popup->RemoveObserver(&observer);

  return HasPopup();
}
