// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_IMPL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/sessions/session_service_base_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class PrefService;
class SessionService;

namespace actions {
class ActionItem;
}  // namespace actions

namespace tabs {

// Implementation for VerticalTabStripStateController.
class VerticalTabStripStateControllerImpl
    : public VerticalTabStripStateController,
      public SessionServiceBaseObserver,
      public BrowserCollectionObserver {
 public:
  explicit VerticalTabStripStateControllerImpl(
      BrowserWindowInterface& browser_window,
      PrefService* pref_service,
      actions::ActionItem* root_action_item,
      SessionService* session_service,
      SessionID session_id,
      std::optional<bool> restored_state_collapsed,
      std::optional<int> restored_state_uncollapsed_width);
  ~VerticalTabStripStateControllerImpl() override;

  void SetDelegate(Delegate* delegate) override;

  bool ShouldDisplayVerticalTabs() const override;
  void SetVerticalTabsEnabled(bool enabled) override;

  std::unique_ptr<ScopedEnableStateLock> GetEnableStateLock() override;

  bool IsCollapsed() const override;
  VerticalTabStripCollapseState GetCollapseState() const override;

  // Request that the Delegate begin transitioning its collapse state.
  // The Delegate is then responsible for updating this class's collapse
  // state through SetCollapsed.
  void RequestCollapse(bool collapse) override;

  int GetUncollapsedWidth() const override;
  void SetUncollapsedWidth(int width) override;

  bool IsExpandOnHoverEnabled() const override;
  void SetExpandOnHoverEnabled(bool enabled) override;

  bool IsResizing() const override;
  void SetIsResizing(bool is_resizing) override;

  const VerticalTabStripState& GetState() const override;

 private:
  class ScopedEnableStateLockImpl;

  void NotifyCollapseChanged();

  void OnModeChanged();
  void OnExpandOnHoverEnabledChanged();

  // Directly sets the collapse state.
  void SetCollapsed(bool collapsed);

  // Updates the SessionService with the current state (collapsed status and
  // uncollapsed width) for the associated session ID.
  void UpdateSessionService();

  // Updates the PrefService with the current state (collapsed status and
  // uncollapsed width) for startup when session restore is not available.
  void UpdatePrefService();

  // Update the Collapse Button's Action Item (kActionToggleCollapseVertical)
  // based on the Vertical Tab Strip's Collapse State.
  void UpdateCollapseActionItem();

  // SessionServiceBase::SessionServiceBaseObserver:
  void OnDestroying(SessionServiceBase* service) override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  void OnDidBecomeActive(BrowserWindowInterface* browser);

  void MaybeShowExpandOnHoverIPH();
  // `new_enabled` represents the updated state of whether vertical tabs is
  // enabled that will be used once fullscreen mode is exited.
  void MaybeShowDelayedToast(bool new_enabled);

  void OnLockCreated();
  void OnLockDestroyed();

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<actions::ActionItem> root_action_item_;
  raw_ptr<SessionService> session_service_;
  const SessionID session_id_;
  const raw_ref<BrowserWindowInterface> browser_window_;
  raw_ptr<Delegate> delegate_;

  // The state of the vertical tabstrip that is persisted to session restore.
  // The collapsed state is true if and only if the tabstrip is fully collapsed.
  // The uncollapsed width is only updated at the end of a resize operation.
  VerticalTabStripState state_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::CallbackListSubscription did_become_active_subscription_;

  bool is_vertical_tabs_enabled_ = false;
  int enable_state_lock_count_ = 0;

  bool is_expand_on_hover_enabled_ = false;
  bool is_resizing_ = false;

  base::OneShotTimer expand_on_hover_iph_startup_timer_;
  base::OneShotTimer expand_on_hover_iph_collapse_timer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_IMPL_H_
