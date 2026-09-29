// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERT_CHILD_TAB_ALERT_HELPER_H_
#define CHROME_BROWSER_UI_TABS_ALERT_CHILD_TAB_ALERT_HELPER_H_

#include <cstddef>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_alert.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;

// Tracks active alerts from child `WebContents` (secondary `WebContents`
// hosted within a tab, such as a Payment Handler dialog, that are not separate
// tabs in the `TabStripModel`).
class ChildTabAlertHelper {
 public:
  DECLARE_USER_DATA(ChildTabAlertHelper);

  explicit ChildTabAlertHelper(TabInterface& tab);
  ChildTabAlertHelper(const ChildTabAlertHelper&) = delete;
  ChildTabAlertHelper& operator=(const ChildTabAlertHelper&) = delete;
  ~ChildTabAlertHelper();

  // `tab` must be non-null.
  static ChildTabAlertHelper* From(TabInterface* tab);

  using ChildAlertsStateChangeCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterChildAlertsStateChange(
      ChildAlertsStateChangeCallback callback);

  // Activates `alert` on the tab for the lifetime of the returned
  // `base::ScopedClosureRunner`. This method must only be called with alert
  // types `kAudioRecording`, `kVideoRecording`, or `kMediaRecording`.
  [[nodiscard]] base::ScopedClosureRunner CreateChildMediaAlert(TabAlert alert);

  // Returns true if `alert` is currently active for any child `WebContents`.
  bool IsChildAlertActive(TabAlert alert) const;

 private:
  void OnChildAlertReleased(TabAlert alert);

  base::RepeatingClosureList child_alerts_state_change_callbacks_;

  // Number of active child alerts per alert type.
  base::flat_map<TabAlert, size_t> child_alert_counts_;

  ui::ScopedUnownedUserData<ChildTabAlertHelper> scoped_unowned_user_data_;

  // Must be the last member.
  base::WeakPtrFactory<ChildTabAlertHelper> weak_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ALERT_CHILD_TAB_ALERT_HELPER_H_
