// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/sessions/session_service_base_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class PrefService;
class SessionService;

namespace actions {
class ActionItem;
}  // namespace actions

namespace tabs {

// The collapse state for the vertical tab strip.
enum class VerticalTabStripCollapseState {
  kCollapsed,
  kCollapsing,
  kExpanded,
};

// The state controller for the vertical tab strip for the browser window. It
// manages the state for the vertical tab strip including display mode, collapse
// state, uncollapsed width and expand to hover setting. It is also responsible
// for serializing the state to the session service.
class VerticalTabStripStateController : public SessionServiceBaseObserver,
                                        public BrowserCollectionObserver {
 public:
  DECLARE_USER_DATA(VerticalTabStripStateController);

  class ScopedEnableStateLock {
   public:
    explicit ScopedEnableStateLock(VerticalTabStripStateController* controller);
    ScopedEnableStateLock(const ScopedEnableStateLock&) = delete;
    ScopedEnableStateLock& operator=(const ScopedEnableStateLock&) = delete;
    ~ScopedEnableStateLock();

   private:
    raw_ptr<VerticalTabStripStateController> controller_;
  };

  // Delegate that is responsible for animating the collapse/expand request, and
  // updating this class's collapse state when it is done.
  class Delegate {
   public:
    virtual void SetCollapsedStateUpdatedCallback(
        base::RepeatingCallback<void(bool)> callback) = 0;
    virtual bool IsCollapsing() = 0;
    virtual void RequestCollapse(bool collapse) = 0;
  };

  explicit VerticalTabStripStateController(
      BrowserWindowInterface* browser_window,
      PrefService* pref_service,
      actions::ActionItem* root_action_item,
      SessionService* session_service,
      SessionID session_id,
      std::optional<bool> restored_state_collapsed,
      std::optional<int> restored_state_uncollapsed_width);
  VerticalTabStripStateController(const VerticalTabStripStateController&) =
      delete;
  VerticalTabStripStateController& operator=(
      const VerticalTabStripStateController&) = delete;
  ~VerticalTabStripStateController() override;

  static const VerticalTabStripStateController* From(
      const BrowserWindowInterface* browser_window);
  static VerticalTabStripStateController* From(
      BrowserWindowInterface* browser_window);

  void SetDelegate(Delegate* delegate);

  bool ShouldDisplayVerticalTabs() const;
  void SetVerticalTabsEnabled(bool enabled);

  std::unique_ptr<ScopedEnableStateLock> GetEnableStateLock();

  bool IsCollapsed() const;
  VerticalTabStripCollapseState GetCollapseState() const;

  // Request that the Delegate begin transitioning its collapse state.
  // The Delegate is then responsible for updating this class's collapse
  // state through SetCollapsed.
  void RequestCollapse(bool collapse);

  int GetUncollapsedWidth() const;
  void SetUncollapsedWidth(int width);

  bool IsExpandOnHoverEnabled() const;
  void SetExpandOnHoverEnabled(bool enabled);

  const VerticalTabStripState& GetState() const { return state_; }

  using CollapseChangeCallback =
      base::RepeatingCallback<void(VerticalTabStripCollapseState)>;
  base::CallbackListSubscription RegisterOnCollapseChanged(
      CollapseChangeCallback callback);

  base::CallbackListSubscription RegisterOnExpandOnHoverEnabledChanged(
      base::RepeatingCallback<void(bool)> callback);

  using StateChangedCallback =
      base::RepeatingCallback<void(VerticalTabStripStateController*)>;
  base::CallbackListSubscription RegisterOnModeWillChange(
      StateChangedCallback callback);
  base::CallbackListSubscription RegisterOnModeChanged(
      StateChangedCallback callback);

  static constexpr char kCollapsedKey[] = "vertical_tab_strip_collapsed";
  static constexpr char kUncollapsedWidthKey[] =
      "vertical_tab_strip_uncollapsed_width";

 private:
  void NotifyCollapseChanged();
  void NotifyExpandOnHoverEnabledChanged();
  void NotifyModeWillChange();
  void NotifyModeChanged();

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

  void MaybeShowExpandOnHoverIPH();

  void OnLockCreated();
  void OnLockDestroyed();

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<actions::ActionItem> root_action_item_;
  raw_ptr<SessionService> session_service_;
  const SessionID session_id_;
  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<Delegate> delegate_;

  // The state of the vertical tabstrip that is persisted to session restore.
  // The collapsed state is true if and only if the tabstrip is fully collapsed.
  // The uncollapsed width is only updated at the end of a resize operation.
  VerticalTabStripState state_;

  base::RepeatingCallbackList<void(VerticalTabStripCollapseState)>
      on_collapse_changed_callback_list_;
  base::RepeatingCallbackList<void(bool)>
      on_expand_on_hover_enabled_changed_callback_list_;
  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_mode_will_change_callback_list_;
  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_mode_changed_callback_list_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  ui::ScopedUnownedUserData<VerticalTabStripStateController>
      scoped_unowned_user_data_;

  bool is_vertical_tabs_enabled_ = false;
  int enable_state_lock_count_ = 0;

  bool is_expand_on_hover_enabled_ = false;

  base::OneShotTimer expand_on_hover_iph_startup_timer_;
  base::OneShotTimer expand_on_hover_iph_collapse_timer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
