// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "ui/actions/actions.h"
#include "ui/views/vector_icons.h"

namespace tabs {

VerticalTabStripStateController::VerticalTabStripStateController(
    PrefService* pref_service,
    actions::ActionItem* root_action_item,
    SessionService* session_service,
    SessionID session_id)
    : pref_service_(pref_service),
      root_action_item_(root_action_item),
      session_service_(session_service),
      session_id_(session_id) {
  pref_change_registrar_.Init(pref_service_);

  pref_change_registrar_.Add(
      prefs::kVerticalTabsEnabled,
      base::BindRepeating(&VerticalTabStripStateController::NotifyStateChanged,
                          base::Unretained(this)));

  UpdateCollapseActionItem();

  if (session_service_) {
    session_service_->AddObserver(this);
  }

  // TODO(crbug.com/455559992): Add uncollapsed text logic for collapse button.
}

VerticalTabStripStateController::~VerticalTabStripStateController() {
  if (session_service_) {
    session_service_->RemoveObserver(this);
    session_service_ = nullptr;
  }
}

bool VerticalTabStripStateController::ShouldDisplayVerticalTabs() const {
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
  if (session_service_) {
    session_service_->AddWindowExtraData(session_id_, kCollapsedKey,
                                         base::ToString(state_.collapsed));
    session_service_->AddWindowExtraData(
        session_id_, kUncollapsedWidthKey,
        base::NumberToString(state_.uncollapsed_width));
  }
  UpdateCollapseActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void VerticalTabStripStateController::UpdateCollapseActionItem() {
  const gfx::VectorIcon& icon =
      IsCollapsed() ? views::kMenuCloseIcon : views::kMenuOpenIcon;

  actions::ActionItem* collapse_action =
      actions::ActionManager::Get().FindAction(kActionToggleCollapseVertical,
                                               root_action_item_);
  if (collapse_action) {
    collapse_action->SetImage(
        ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
  }
}

void VerticalTabStripStateController::OnDestroying(
    SessionServiceBase* service) {
  if (service == session_service_) {
    session_service_->RemoveObserver(this);
    session_service_ = nullptr;
  }
}

}  // namespace tabs
