// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/boca_window_observer.h"
#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_extensions_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "url/gurl.h"

namespace ash::boca {

class OnTaskSessionManagerTest;

// Session manager implementation that is primarily used for configuring and
// managing OnTask components and services throughout a Boca session.
class OnTaskSessionManager : public boca::BocaSessionManager::Observer,
                             public boca::BocaWindowObserver {
 public:
  explicit OnTaskSessionManager(
      std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager,
      std::unique_ptr<OnTaskExtensionsManager> extensions_manager);
  OnTaskSessionManager(const OnTaskSessionManager&) = delete;
  OnTaskSessionManager& operator=(const OnTaskSessionManager&) = delete;
  ~OnTaskSessionManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;
  void OnBundleUpdated(const ::boca::Bundle& bundle) override;
  void OnAppReloaded() override;

  ActiveTabTracker* active_tab_tracker() { return &active_tab_tracker_; }

  // BocaWindowObserver:
  void OnTabAdded(const SessionID active_tab_id,
                  const SessionID tab_id,
                  const GURL url) override;
  void OnTabRemoved(const SessionID tab_id) override;

 private:
  friend class OnTaskSessionManagerTest;

  // Helper class that is used to launch the Boca system web app as well as
  // manage all interactions with the Boca system web app while it is being
  // spawned.
  class SystemWebAppLaunchHelper {
   public:
    SystemWebAppLaunchHelper(
        OnTaskSystemWebAppManager* system_web_app_manager,
        const std::vector<boca::BocaWindowObserver*> observers);
    SystemWebAppLaunchHelper(const SystemWebAppLaunchHelper&) = delete;
    SystemWebAppLaunchHelper& operator=(const SystemWebAppLaunchHelper&) =
        delete;
    ~SystemWebAppLaunchHelper();

    void LaunchBocaSWA();
    void AddTab(
        GURL url,
        ::boca::LockedNavigationOptions::NavigationType restriction_level,
        base::OnceCallback<void(SessionID)> callback);
    void RemoveTab(const std::set<SessionID>& tab_ids_to_remove,
                   base::OnceClosure callback);
    void SetPinStateForActiveSWAWindow(bool pinned,
                                       base::RepeatingClosure callback);

   private:
    // Callback triggered when the Boca SWA is launched. Normally at the onset
    // of a Boca session.
    void OnBocaSWALaunched(bool success);

    // Owned by the parent class `OnTaskSessionManager` that owns an instance of
    // the class `SystemWebAppLaunchHelper`, so there won't be UAF errors.
    raw_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;
    std::vector<boca::BocaWindowObserver*> observers_;

    SEQUENCE_CHECKER(sequence_checker_);

    bool launch_in_progress_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

    base::WeakPtrFactory<SystemWebAppLaunchHelper> weak_ptr_factory_{this};
  };

  // Internal helper used to lock or unlock the current app window. This
  // involves disabling relevant extensions and pinning the window if
  // `lock_window` is true, or re-enabling extensions and unpinning the window
  // otherwise.
  void LockOrUnlockWindow(bool lock_window);

  // Callback triggered when a tab from the bundle is added.
  void OnBundleTabAdded(
      GURL url,
      ::boca::LockedNavigationOptions::NavigationType restriction_level,
      SessionID tab_id);

  // Callback triggered when a tab from the bundle is removed.
  void OnBundleTabRemoved(GURL url);

  // Callback triggered when the Boca SWA window pin state is set.
  void OnSetPinStateOnBocaSWAWindow();

  // Set the `active_tab_url_` to be the url associated with `tab_id`.
  void TrackActiveTabURLFromTab(SessionID tab_id);

  ActiveTabTracker active_tab_tracker_;

  const std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<std::string> active_session_id_
      GUARDED_BY_CONTEXT(sequence_checker_) = std::nullopt;
  GURL active_tab_url_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool should_lock_window_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Maps the url that providers send to the tab ids spawned from the url. This
  // map allows to remove all the related tabs to the url.
  base::flat_map<GURL, std::set<SessionID>> provider_url_tab_ids_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Maps the url that providers send to the restriction levels it is currently
  // set to. This map allows for tracking restriction level updates.
  base::flat_map<GURL, ::boca::LockedNavigationOptions::NavigationType>
      provider_url_restriction_level_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  const std::unique_ptr<OnTaskExtensionsManager> extensions_manager_;

  const std::unique_ptr<SystemWebAppLaunchHelper> system_web_app_launch_helper_;

  std::unique_ptr<OnTaskNotificationsManager> notifications_manager_;

  base::WeakPtrFactory<OnTaskSessionManager> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
