// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"

#include <optional>

#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

namespace tabs {

DEFINE_USER_DATA(VerticalTabStripStateController);

VerticalTabStripStateController::VerticalTabStripStateController(
    BrowserWindowInterface* browser_window,
    PrefService* pref_service,
    actions::ActionItem* root_action_item,
    SessionService* session_service,
    SessionID session_id,
    std::optional<bool> restored_state_collapsed,
    std::optional<int> restored_state_uncollapsed_width)
    : pref_service_(pref_service),
      root_action_item_(root_action_item),
      session_service_(session_service),
      session_id_(session_id),
      browser_window_(browser_window),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  pref_change_registrar_.Init(pref_service_);

  pref_change_registrar_.Add(
      prefs::kVerticalTabsEnabled,
      base::BindRepeating(&VerticalTabStripStateController::OnModeChanged,
                          base::Unretained(this)));

  if (restored_state_collapsed.has_value()) {
    SetCollapsed(restored_state_collapsed.value());
  }
  if (restored_state_uncollapsed_width.has_value()) {
    SetUncollapsedWidth(restored_state_uncollapsed_width.value());
  }

  UpdateCollapseActionItem();

  if (session_service_) {
    session_service_->AddObserver(this);

    bool is_browser_ready = false;
    GlobalBrowserCollection::GetInstance()->ForEach(
        [&is_browser_ready, this](BrowserWindowInterface* browser) {
          if (browser->GetSessionID() == session_id_) {
            is_browser_ready = true;
          }
          return !is_browser_ready;
        });

    if (!is_browser_ready) {
      browser_collection_observation_.Observe(
          GlobalBrowserCollection::GetInstance());
    }
  }
}

VerticalTabStripStateController::~VerticalTabStripStateController() {
  if (session_service_) {
    session_service_->RemoveObserver(this);
    session_service_ = nullptr;
  }
}

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

bool VerticalTabStripStateController::ShouldDisplayVerticalTabs() const {
  return IsVerticalTabsFeatureEnabled() &&
         pref_service_->GetBoolean(prefs::kVerticalTabsEnabled);
}

void VerticalTabStripStateController::SetVerticalTabsEnabled(bool enabled) {
  NotifyModeWillChange();
  pref_service_->SetBoolean(prefs::kVerticalTabsEnabled, enabled);
}

bool VerticalTabStripStateController::IsCollapsed() const {
  return state_.collapsed;
}

void VerticalTabStripStateController::SetCollapsed(bool collapsed) {
  if (state_.collapsed != collapsed) {
    state_.collapsed = collapsed;
    NotifyCollapseChanged();
  }
}

int VerticalTabStripStateController::GetUncollapsedWidth() const {
  return state_.uncollapsed_width;
}

void VerticalTabStripStateController::SetUncollapsedWidth(int width) {
  if (state_.uncollapsed_width != width) {
    state_.uncollapsed_width = width;
    NotifyCollapseChanged();
  }
}

void VerticalTabStripStateController::SetState(
    const VerticalTabStripState& state) {
  if (state_.collapsed != state.collapsed ||
      state_.uncollapsed_width != state.uncollapsed_width) {
    state_ = state;
    NotifyCollapseChanged();
  }
}

base::CallbackListSubscription
VerticalTabStripStateController::RegisterOnCollapseChanged(
    StateChangedCallback callback) {
  return on_collapse_changed_callback_list_.Add(std::move(callback));
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

void VerticalTabStripStateController::NotifyCollapseChanged() {
  UpdateSessionService();
  UpdateCollapseActionItem();
  on_collapse_changed_callback_list_.Notify(this);
}

void VerticalTabStripStateController::NotifyModeWillChange() {
  on_mode_will_change_callback_list_.Notify(this);
}

void VerticalTabStripStateController::NotifyModeChanged() {
  on_mode_changed_callback_list_.Notify(this);
}

void VerticalTabStripStateController::OnModeChanged() {
  if (pref_service_->GetBoolean(prefs::kVerticalTabsEnabled) &&
      !pref_service_->GetBoolean(prefs::kVerticalTabsEnabledFirstTime)) {
    base::RecordAction(
        base::UserMetricsAction("VerticalTabs_EnabledFirstTime"));
    pref_service_->SetBoolean(prefs::kVerticalTabsEnabledFirstTime, true);
  }
  NotifyModeChanged();
}

void VerticalTabStripStateController::UpdateSessionService() {
  if (session_service_ && !browser_collection_observation_.IsObserving()) {
    session_service_->AddWindowExtraData(session_id_, kCollapsedKey,
                                         base::ToString(state_.collapsed));
    session_service_->AddWindowExtraData(
        session_id_, kUncollapsedWidthKey,
        base::NumberToString(state_.uncollapsed_width));
  }
}

void VerticalTabStripStateController::UpdateCollapseActionItem() {
  const gfx::VectorIcon& icon = (IsCollapsed() == base::i18n::IsRTL())
                                    ? views::kMenuOpenIcon
                                    : views::kMenuCloseIcon;

  const auto& text =
      IsCollapsed() ? IDS_EXPAND_VERTICAL_TABS : IDS_COLLAPSE_VERTICAL_TABS;

  actions::ActionItem* collapse_action =
      actions::ActionManager::Get().FindAction(kActionToggleCollapseVertical,
                                               root_action_item_);
  if (collapse_action) {
    collapse_action->SetImage(
        ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
    collapse_action->SetText(BrowserActions::GetCleanTitleAndTooltipText(
        l10n_util::GetStringUTF16(text)));
    collapse_action->SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
        l10n_util::GetStringUTF16(text)));
  }
}

void VerticalTabStripStateController::OnDestroying(
    SessionServiceBase* service) {
  if (service == session_service_) {
    session_service_->RemoveObserver(this);
    session_service_ = nullptr;
  }
}

void VerticalTabStripStateController::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (browser == browser_window_) {
    browser_collection_observation_.Reset();
    UpdateSessionService();
  }
}

}  // namespace tabs
