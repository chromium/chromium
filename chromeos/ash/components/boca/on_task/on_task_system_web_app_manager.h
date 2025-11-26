// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_

#include "ash/webui/boca_ui/url_constants.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_window_observer.h"
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

  // Launches the Boca SWA with homepage url and triggers the specified callback
  // to convey the caller if the launch succeeded.
  virtual void LaunchSystemWebAppAsync(
      base::OnceCallback<void(bool)> callback,
      const GURL& url = GURL(kChromeBocaAppUntrustedIndexURL)) = 0;

  // Closes the specified Boca SWA window.
  virtual void CloseSystemWebAppWindow(SessionID window_id) = 0;

  // Returns a valid window id associated with the active Boca SWA window. If
  // there is no such active window, it returns `SessionID::InvalidValue()`.
  virtual SessionID GetActiveSystemWebAppWindowID() = 0;

  // Pins/unpins the specified Boca SWA window based on the specified value.
  virtual void SetPinStateForSystemWebAppWindow(bool pinned,
                                                SessionID window_id) = 0;

  // Pause/unpause the specified Boca SWA window based on the specified value.
  virtual void SetPauseStateForSystemWebAppWindow(bool paused,
                                                  SessionID window_id) = 0;

  // Set the window tracker to track the browser browser window with specified
  // id.
  virtual void SetWindowTrackerForSystemWebAppWindow(
      SessionID window_id,
      const std::vector<boca::BocaWindowObserver*> observers) = 0;

  // Creates a background tab with the given URL and restriction_level in the
  // specified Boca SWA window.
  virtual SessionID CreateBackgroundTabWithUrl(
      SessionID window_id,
      GURL url,
      ::boca::LockedNavigationOptions::NavigationType restriction_level) = 0;

  // Removes tabs with the given tab ids in the specified Boca SWA window.
  virtual void RemoveTabsWithTabIds(
      SessionID window_id,
      const std::set<SessionID>& tab_ids_to_remove) = 0;

  // Set restriction_level for the tabs in the specified Boca SWA window.
  virtual void SetParentTabsRestriction(
      SessionID window_id,
      ::boca::LockedNavigationOptions::NavigationType restriction_level) = 0;

  // Sets up the specified Boca SWA window for OnTask. Setting
  // `close_bundle_content` will remove all pre-existing content tabs except for
  // the homepage one, normally required at the onset of a new session or when
  // the app instance was restored manually in the middle of an active session.
  virtual void PrepareSystemWebAppWindowForOnTask(
      SessionID window_id,
      bool close_bundle_content) = 0;

  // Returns a valid tab id associated with the active tab. If
  // there is no valid active tab, it returns `SessionID::InvalidValue()`.
  virtual SessionID GetActiveTabID() = 0;

  // Switch to the tab with associated with `tab_id`.
  virtual void SwitchToTab(SessionID tab_id) = 0;

  // Mute/unmute all tabs in all browser instances.
  virtual void SetAllChromeTabsMuted(bool muted) = 0;

  // If current window is in lock/pinned state.
  virtual bool IsWindowPinned(SessionID window_id) = 0;

 protected:
  OnTaskSystemWebAppManager() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_
