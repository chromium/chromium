// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sessions/session_service_base_observer.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"

class PrefService;
class SessionService;

namespace actions {
class ActionItem;
}  // namespace actions

namespace tabs {

class VerticalTabStripStateController : public SessionServiceBaseObserver {
 public:
  explicit VerticalTabStripStateController(
      PrefService* pref_service,
      actions::ActionItem* root_action_item,
      SessionService* session_service,
      SessionID session_id);
  VerticalTabStripStateController(const VerticalTabStripStateController&) =
      delete;
  VerticalTabStripStateController& operator=(
      const VerticalTabStripStateController&) = delete;
  ~VerticalTabStripStateController() override;

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

  // Update the Collapse Button's Action Item (kActionToggleCollapseVertical)
  // based on the Vertical Tab Strip's Collapse State.
  void UpdateCollapseActionItem();

  // SessionServiceBase::SessionServiceBaseObserver:
  void OnDestroying(SessionServiceBase* service) override;

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<actions::ActionItem> root_action_item_;
  raw_ptr<SessionService> session_service_;
  VerticalTabStripState state_;
  const SessionID session_id_;
  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_state_changed_callback_list_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
