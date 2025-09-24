// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace tabs {

class VerticalTabStripStateController {
 public:
  explicit VerticalTabStripStateController(PrefService* pref_service);
  VerticalTabStripStateController(const VerticalTabStripStateController&) =
      delete;
  VerticalTabStripStateController& operator=(
      const VerticalTabStripStateController&) = delete;
  ~VerticalTabStripStateController();

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

 private:
  void NotifyStateChanged();

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  VerticalTabStripState state_;
  base::RepeatingCallbackList<void(VerticalTabStripStateController*)>
      on_state_changed_callback_list_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
