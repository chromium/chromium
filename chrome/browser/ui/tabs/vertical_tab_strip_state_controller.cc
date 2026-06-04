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
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
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

  is_vertical_tabs_enabled_ =
      pref_service_->GetBoolean(prefs::kVerticalTabsEnabled);

  pref_change_registrar_.Add(
      prefs::kVerticalTabsEnabled,
      base::BindRepeating(&VerticalTabStripStateController::OnModeChanged,
                          base::Unretained(this)));

  if (IsVerticalTabsExpandOnHoverFeatureEnabled()) {
    is_expand_on_hover_enabled_ =
        pref_service_->GetBoolean(prefs::kVerticalTabsExpandOnHoverEnabled);

    pref_change_registrar_.Add(
        prefs::kVerticalTabsExpandOnHoverEnabled,
        base::BindRepeating(
            &VerticalTabStripStateController::OnExpandOnHoverEnabledChanged,
            base::Unretained(this)));
  }

  if (restored_state_collapsed.has_value()) {
    SetCollapsed(restored_state_collapsed.value());
  }
  if (restored_state_uncollapsed_width.has_value()) {
    SetUncollapsedWidth(restored_state_uncollapsed_width.value());
  }

  UpdateCollapseActionItem();

  // Observe active browser to update most recently used vertical tabs pref
  // accordingly.
  did_become_active_subscription_ = browser_window_->RegisterDidBecomeActive(
      base::BindRepeating(&VerticalTabStripStateController::OnDidBecomeActive,
                          base::Unretained(this)));

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

  // Wait before attempting to show the expand on hover IPH on startup. This is
  // an anti-pattern and is generally discouraged for snooze promos.
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    expand_on_hover_iph_collapse_timer_.Start(
        FROM_HERE, base::Minutes(10),
        base::BindOnce(
            &VerticalTabStripStateController::MaybeShowExpandOnHoverIPH,
            base::Unretained(this)));
  }
}

VerticalTabStripStateController::~VerticalTabStripStateController() {
  if (session_service_) {
    session_service_->RemoveObserver(this);
    session_service_ = nullptr;
  }

  // Prevent dangling pointer.
  if (delegate_) {
    delegate_->SetCollapsedStateUpdatedCallback(base::DoNothing());
  }
}

VerticalTabStripStateController::ScopedEnableStateLock::ScopedEnableStateLock(
    VerticalTabStripStateController* controller)
    : controller_(controller) {
  if (controller_) {
    controller_->OnLockCreated();
  }
}

VerticalTabStripStateController::ScopedEnableStateLock::
    ~ScopedEnableStateLock() {
  if (controller_) {
    controller_->OnLockDestroyed();
  }
}

std::unique_ptr<VerticalTabStripStateController::ScopedEnableStateLock>
VerticalTabStripStateController::GetEnableStateLock() {
  return std::make_unique<ScopedEnableStateLock>(this);
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

void VerticalTabStripStateController::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (delegate_) {
    delegate_->SetCollapsedStateUpdatedCallback(
        base::BindRepeating(&VerticalTabStripStateController::SetCollapsed,
                            base::Unretained(this)));
  }
}

bool VerticalTabStripStateController::ShouldDisplayVerticalTabs() const {
  return IsVerticalTabsFeatureEnabled() && is_vertical_tabs_enabled_;
}

void VerticalTabStripStateController::SetVerticalTabsEnabled(bool enabled) {
  NotifyModeWillChange();
  pref_service_->SetBoolean(prefs::kVerticalTabsEnabled, enabled);
}

bool VerticalTabStripStateController::IsCollapsed() const {
  return state_.collapsed;
}

VerticalTabStripCollapseState
VerticalTabStripStateController::GetCollapseState() const {
  if (IsCollapsed()) {
    return VerticalTabStripCollapseState::kCollapsed;
  }
  if (delegate_ && delegate_->IsCollapsing()) {
    return VerticalTabStripCollapseState::kCollapsing;
  }
  return VerticalTabStripCollapseState::kExpanded;
}

void VerticalTabStripStateController::RequestCollapse(bool collapse) {
  if (!delegate_) {
    return;
  }

  delegate_->RequestCollapse(collapse);

  if (delegate_->IsCollapsing()) {
    NotifyCollapseChanged();
  }

  // Wait a short duration before attempting to show the expand on hover IPH
  // after collapsing.
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    expand_on_hover_iph_startup_timer_.Start(
        FROM_HERE, base::Seconds(3),
        base::BindOnce(
            &VerticalTabStripStateController::MaybeShowExpandOnHoverIPH,
            base::Unretained(this)));
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

bool VerticalTabStripStateController::IsExpandOnHoverEnabled() const {
  return is_expand_on_hover_enabled_;
}

void VerticalTabStripStateController::SetExpandOnHoverEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kVerticalTabsExpandOnHoverEnabled, enabled);
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

void VerticalTabStripStateController::NotifyCollapseChanged() {
  UpdateCollapseActionItem();
  UpdateSessionService();
  UpdatePrefService();
  on_collapse_changed_callback_list_.Notify(GetCollapseState());
}

void VerticalTabStripStateController::NotifyExpandOnHoverEnabledChanged() {
  on_expand_on_hover_enabled_changed_callback_list_.Notify(
      is_expand_on_hover_enabled_);
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

  if (enable_state_lock_count_ > 0) {
    return;
  }

  is_vertical_tabs_enabled_ =
      pref_service_->GetBoolean(prefs::kVerticalTabsEnabled);

  NotifyModeChanged();
}

void VerticalTabStripStateController::OnExpandOnHoverEnabledChanged() {
  is_expand_on_hover_enabled_ =
      pref_service_->GetBoolean(prefs::kVerticalTabsExpandOnHoverEnabled);
  if (is_expand_on_hover_enabled_) {
    base::RecordAction(
        base::UserMetricsAction("VerticalTabs_ExpandOnHover_Enabled"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("VerticalTabs_ExpandOnHover_Disabled"));
  }
  NotifyExpandOnHoverEnabledChanged();
}

void VerticalTabStripStateController::SetCollapsed(bool collapsed) {
  if (state_.collapsed != collapsed) {
    state_.collapsed = collapsed;
    NotifyCollapseChanged();
    if (auto* browser_element_views =
            BrowserElementsViews::From(browser_window_);
        collapsed && browser_element_views) {
      browser_element_views->NotifyEvent(
          kTabStripRegionElementId, kVerticalTabStripCollapsedCustomEventId);
    }
  }
}

void VerticalTabStripStateController::OnLockCreated() {
  enable_state_lock_count_++;
}

void VerticalTabStripStateController::OnLockDestroyed() {
  CHECK_GT(enable_state_lock_count_, 0);
  enable_state_lock_count_--;

  if (enable_state_lock_count_ == 0) {
    bool new_state = pref_service_->GetBoolean(prefs::kVerticalTabsEnabled);
    if (new_state != is_vertical_tabs_enabled_) {
      is_vertical_tabs_enabled_ = new_state;
      NotifyModeChanged();
    }
  }
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

void VerticalTabStripStateController::UpdatePrefService() {
  if (pref_service_) {
    pref_service_->SetBoolean(prefs::kVerticalTabsCollapsedState,
                              state_.collapsed);
    pref_service_->SetInteger(prefs::kVerticalTabsUncollapsedWidth,
                              state_.uncollapsed_width);
  }
}

void VerticalTabStripStateController::UpdateCollapseActionItem() {
  const bool is_collapsed =
      GetCollapseState() != VerticalTabStripCollapseState::kExpanded;

  const gfx::VectorIcon& icon = (is_collapsed == base::i18n::IsRTL())
                                    ? features::IsRoundedIconsEnabled()
                                          ? views::kMenuOpenIcon
                                          : views::kMenuOpenOldIcon
                                    : views::kMenuCloseCustomIcon;

  const auto& text =
      is_collapsed ? IDS_EXPAND_VERTICAL_TABS : IDS_COLLAPSE_VERTICAL_TABS;

  actions::ActionItem* collapse_action =
      actions::ActionManager::Get().FindAction(kActionToggleCollapseVertical,
                                               root_action_item_);
  if (collapse_action) {
    collapse_action->SetImage(
        ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
    collapse_action->SetText(
        chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text)));
    collapse_action->SetTooltipText(
        chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text)));
  }
}

void VerticalTabStripStateController::OnDidBecomeActive(
    BrowserWindowInterface* browser) {
  UpdatePrefService();
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
    UpdatePrefService();
  }
}

void VerticalTabStripStateController::MaybeShowExpandOnHoverIPH() {
  if (tabs::IsVerticalTabsExpandOnHoverFeatureEnabled() &&
      ShouldDisplayVerticalTabs() &&
      GetCollapseState() != VerticalTabStripCollapseState::kExpanded &&
      pref_service_->FindPreference(prefs::kVerticalTabsExpandOnHoverEnabled)
          ->IsDefaultValue()) {
    BrowserUserEducationInterface::From(browser_window_)
        ->MaybeShowFeaturePromo(
            feature_engagement::kIPHVerticalTabsExpandOnHoverFeature);
  }
}

}  // namespace tabs
