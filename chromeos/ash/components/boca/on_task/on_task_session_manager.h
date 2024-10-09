// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_extensions_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "url/gurl.h"

namespace ash::boca {

// Session manager implementation that is primarily used for configuring and
// managing OnTask components and services throughout a Boca session.
class OnTaskSessionManager : public boca::BocaSessionManager::Observer {
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
  ActiveTabTracker* active_tab_tracker() { return &active_tab_tracker_; }

 private:
  // Helper class that is used to launch the Boca system web app as well as
  // manage all interactions with the Boca system web app while it is being
  // spawned.
  class SystemWebAppLaunchHelper {
   public:
    SystemWebAppLaunchHelper(OnTaskSystemWebAppManager* system_web_app_manager,
                             ActiveTabTracker* tracker);
    SystemWebAppLaunchHelper(const SystemWebAppLaunchHelper&) = delete;
    SystemWebAppLaunchHelper& operator=(const SystemWebAppLaunchHelper&) =
        delete;
    ~SystemWebAppLaunchHelper();

    void LaunchBocaSWA();
    void AddTab(GURL url,
                OnTaskBlocklist::RestrictionLevel restriction_level,
                base::OnceCallback<void(SessionID)> callback);
    void RemoveTab(const base::flat_set<SessionID>& tab_ids_to_remove,
                   base::OnceClosure callback);

   private:
    // Callback triggered when the Boca SWA is launched. Normally at the onset
    // of a Boca session.
    void OnBocaSWALaunched(bool success);

    // Owned by the parent class `OnTaskSessionManager` that owns an instance of
    // the class `SystemWebAppLaunchHelper`, so there won't be UAF errors.
    raw_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;
    raw_ptr<ActiveTabTracker> active_tab_tracker_;

    SEQUENCE_CHECKER(sequence_checker_);

    bool launch_in_progress_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

    base::WeakPtrFactory<SystemWebAppLaunchHelper> weak_ptr_factory_{this};
  };

  // Callback triggered when a tab is added.
  void OnTabAdded(GURL url, SessionID tab_id);

  // Callback triggered when a tab is removed.
  void OnTabRemoved(GURL url);
  ActiveTabTracker active_tab_tracker_;

  const std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Maps the url that providers send to the tab ids spawned from the url. This
  // map allows to remove all the related tabs to the url.
  base::flat_map<GURL, base::flat_set<SessionID>> provider_url_tab_ids_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const std::unique_ptr<OnTaskExtensionsManager> extensions_manager_;

  const std::unique_ptr<SystemWebAppLaunchHelper> system_web_app_launch_helper_;

  base::WeakPtrFactory<OnTaskSessionManager> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
