// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace ash::boca {

// Responsible for managing all OnTask interactions with the Boca SWA. These
// interactions include launching the Boca SWA, closing the active SWA instance,
// pinning/unpinning the active SWA instance.
class OnTaskSystemWebAppManager {
 public:
  OnTaskSystemWebAppManager(const OnTaskSystemWebAppManager&) = delete;
  OnTaskSystemWebAppManager& operator=(const OnTaskSystemWebAppManager&) =
      delete;
  virtual ~OnTaskSystemWebAppManager() = default;

  // Launches the Boca SWA and triggers the specified callback to convey the
  // caller if the launch succeeded.
  virtual void LaunchSystemWebAppAsync(
      base::OnceCallback<void(bool)> callback) = 0;

  // Closes the specified Boca SWA window.
  virtual void CloseSystemWebAppWindow(SessionID window_id) = 0;

  // Returns a valid window id associated with the active Boca SWA window. If
  // there is no such active window, it returns `SessionID::InvalidValue()`.
  virtual SessionID GetActiveSystemWebAppWindowID() = 0;

  // Pins/unpins the specified Boca SWA window based on the specified value.
  virtual void SetPinStateForSystemWebAppWindow(bool pinned,
                                                SessionID window_id) = 0;

  // Set the window tracker to track the browser browser window with specified
  // id.
  virtual void SetWindowTrackerForSystemWebAppWindow(
      SessionID window_id,
      ActiveTabTracker* observer) = 0;

  // Creates a background tab with the given URL and restriction_level in the
  // specified Boca SWA window.
  virtual SessionID CreateBackgroundTabWithUrl(
      SessionID window_id,
      GURL url,
      OnTaskBlocklist::RestrictionLevel restriction_level) = 0;

  // Removes tabs with the given tab ids in the specified Boca SWA window.
  virtual void RemoveTabsWithTabIds(
      SessionID window_id,
      const base::flat_set<SessionID>& tab_ids_to_remove) = 0;

 protected:
  OnTaskSystemWebAppManager() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_
