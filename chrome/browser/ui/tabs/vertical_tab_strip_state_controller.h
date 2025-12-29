// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/sessions/session_service_base_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserList;
class BrowserWindowInterface;
class PrefService;
class SessionService;

namespace actions {
class ActionItem;
}  // namespace actions

namespace tabs {

class VerticalTabStripStateController : public SessionServiceBaseObserver,
                                        public BrowserListObserver {
 public:
  DECLARE_USER_DATA(VerticalTabStripStateController);

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

  static VerticalTabStripStateController* From(
      BrowserWindowInterface* browser_window);

  bool ShouldDisplayVerticalTabs() const;
  void SetVerticalTabsEnabled(bool enabled);

  bool IsCollapsed() const;
  void SetCollapsed(bool collapsed);

  int GetUncollapsedWidth() const;
  void SetUncollapsedWidth(int width);

  const VerticalTabStripState& GetState() const { return state_; }
  void SetState(const VerticalTabStripState& state);

  using StateChangedCallback =
      base::RepeatingCallback<void(VerticalTabStripStateController*)>;
  base::CallbackListSubscription RegisterOnStateChanged(
      StateChangedCallback callback);

  static constexpr char kCollapsedKey[] = "vertical_tab_strip_collapsed";
  static constexpr char kUncollapsedWidthKey[] =
      "vertical_tab_strip_uncollapsed_width";

 private:
  void NotifyStateChanged();

  // Updates the SessionService with the current state (collapsed status and
  // uncollapsed width) for the associated session ID.
  void UpdateSessionService();

  // Update the Collapse Button's Action Item (kActionToggleCollapseVertical)
  // based on the Vertical Tab Strip's Collapse State.
  void UpdateCollapseActionItem();

  // SessionServiceBase::SessionServiceBaseObserver:
  void OnDestroying(SessionServiceBase* service) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<actions::ActionItem> root_action_item_;
  raw_ptr<SessionService> session_service_;
  const SessionID session_id_;
  raw_ptr<BrowserWindowInterface> browser_window_;

  VerticalTabStripState state_;

  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_state_changed_callback_list_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  ui::ScopedUnownedUserData<VerticalTabStripStateController>
      scoped_unowned_user_data_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
