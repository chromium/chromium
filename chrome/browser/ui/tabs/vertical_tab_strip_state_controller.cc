// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace tabs {

VerticalTabStripStateController::VerticalTabStripStateController(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

VerticalTabStripStateController::~VerticalTabStripStateController() = default;

bool VerticalTabStripStateController::IsVerticalTabsEnabled() const {
  return pref_service_->GetBoolean(prefs::kVerticalTabsEnabled);
}

void VerticalTabStripStateController::SetVerticalTabsEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kVerticalTabsEnabled, enabled);
}

bool VerticalTabStripStateController::IsCollapsed() const {
  return state_.collapsed;
}

void VerticalTabStripStateController::SetCollapsed(bool collapsed) {
  if (state_.collapsed != collapsed) {
    state_.collapsed = collapsed;
    NotifyStateChanged();
  }
}

int VerticalTabStripStateController::GetUncollapsedWidth() const {
  return state_.uncollapsed_width;
}

void VerticalTabStripStateController::SetUncollapsedWidth(int width) {
  if (state_.uncollapsed_width != width) {
    state_.uncollapsed_width = width;
    NotifyStateChanged();
  }
}

void VerticalTabStripStateController::SetState(
    const VerticalTabStripState& state) {
  if (state_.collapsed != state.collapsed ||
      state_.uncollapsed_width != state.uncollapsed_width) {
    state_ = state;
    NotifyStateChanged();
  }
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

void VerticalTabStripStateController::NotifyStateChanged() {
  on_state_changed_callback_list_.Notify(this);
}

}  // namespace tabs
