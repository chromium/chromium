// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/focus/browser_focus_controller.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/base_window.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

DEFINE_USER_DATA(BrowserFocusController);

BrowserFocusController::BrowserFocusController(ui::BaseWindow* base_window,
                                               ui::UnownedUserDataHost& host)
    : base_window_(CHECK_DEREF(base_window)),
      scoped_data_holder_(host, *this) {}

BrowserFocusController::~BrowserFocusController() = default;

// static
BrowserFocusController* BrowserFocusController::From(
    BrowserWindowInterface* browser) {
  CHECK(browser);
  return ui::ScopedUnownedUserData<BrowserFocusController>::Get(
      browser->GetUnownedUserDataHost());
}

// static
const BrowserFocusController* BrowserFocusController::From(
    const BrowserWindowInterface* browser) {
  CHECK(browser);
  return ui::ScopedUnownedUserData<BrowserFocusController>::Get(
      browser->GetUnownedUserDataHost());
}

void BrowserFocusController::SetDelegate(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void BrowserFocusController::RotatePaneFocus(bool forwards) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(base_window_->GetNativeWindow());
  if (!widget) {
    return;
  }
  widget->GetFocusManager()->RotatePaneFocus(
      forwards ? views::FocusManager::Direction::kForward
               : views::FocusManager::Direction::kBackward,
      views::FocusManager::FocusCycleWrapping::kEnabled);
}

void BrowserFocusController::FocusWebContentsPane() {
  if (delegate_) {
    delegate_->FocusWebContentsPane();
  }
}

void BrowserFocusController::FocusInactivePopupForAccessibility() {
  if (delegate_) {
    delegate_->FocusInactivePopupForAccessibility();
  }
}

bool BrowserFocusController::ActivateFirstInactiveBubbleForAccessibility() {
  if (delegate_) {
    return delegate_->ActivateFirstInactiveBubbleForAccessibility();
  }
  return false;
}
