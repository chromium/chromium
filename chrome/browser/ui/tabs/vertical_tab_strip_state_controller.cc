// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace tabs {

DEFINE_USER_DATA(VerticalTabStripStateController);

VerticalTabStripStateController::VerticalTabStripStateController(
    BrowserWindowInterface& browser)
    : scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {}
VerticalTabStripStateController::~VerticalTabStripStateController() = default;

// static
const VerticalTabStripStateController* VerticalTabStripStateController::From(
    const BrowserWindowInterface* browser_window) {
  return browser_window ? Get(browser_window->GetUnownedUserDataHost())
                        : nullptr;
}

// static
VerticalTabStripStateController* VerticalTabStripStateController::From(
    BrowserWindowInterface* browser_window) {
  return browser_window ? Get(browser_window->GetUnownedUserDataHost())
                        : nullptr;
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnResizingChanged(
    ResizingChangeCallback callback) {
  return on_resizing_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnCollapseChanged(
    CollapseChangeCallback callback) {
  return on_collapse_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnExpandOnHoverEnabledChanged(
    base::RepeatingCallback<void(bool)> callback) {
  return on_expand_on_hover_enabled_changed_callback_list_.Add(
      std::move(callback));
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnModeWillChange(
    StateChangedCallback callback) {
  return on_mode_will_change_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnModeChanged(
    StateChangedCallback callback) {
  return on_mode_changed_callback_list_.Add(std::move(callback));
}

void VerticalTabStripStateController::NotifyResizingChanged(bool is_resizing) {
  on_resizing_changed_callback_list_.Notify(is_resizing);
}

void VerticalTabStripStateController::NotifyCollapseChanged(
    VerticalTabStripCollapseState new_state) {
  on_collapse_changed_callback_list_.Notify(new_state);
}

void VerticalTabStripStateController::NotifyExpandOnHoverEnabledChanged(
    bool expand_on_hover_enabled) {
  on_expand_on_hover_enabled_changed_callback_list_.Notify(
      expand_on_hover_enabled);
}

void VerticalTabStripStateController::NotifyModeWillChange() {
  on_mode_will_change_callback_list_.Notify(this);
}

void VerticalTabStripStateController::NotifyModeChanged() {
  on_mode_changed_callback_list_.Notify(this);
}

}  // namespace tabs
