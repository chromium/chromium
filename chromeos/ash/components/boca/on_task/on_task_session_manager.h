// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "url/gurl.h"

namespace ash::boca {

// Session manager implementation that is primarily used for configuring and
// managing OnTask components and services throughout a Boca session.
class OnTaskSessionManager : public boca::BocaSessionManager::Observer {
 public:
  explicit OnTaskSessionManager(
      std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager);
  OnTaskSessionManager(const OnTaskSessionManager&) = delete;
  OnTaskSessionManager& operator=(const OnTaskSessionManager&) = delete;
  ~OnTaskSessionManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;
  void OnBundleUpdated(const ::boca::Bundle& bundle) override;

 private:
  // Helper class that is used to launch the Boca system web app as well as
  // manage all interactions with the Boca system web app while it is being
  // spawned.
  class SystemWebAppLaunchHelper {
   public:
    SystemWebAppLaunchHelper(OnTaskSystemWebAppManager* system_web_app_manager);
    SystemWebAppLaunchHelper(const SystemWebAppLaunchHelper&) = delete;
    SystemWebAppLaunchHelper& operator=(const SystemWebAppLaunchHelper&) =
        delete;
    ~SystemWebAppLaunchHelper();

    void LaunchBocaSWA();
    void AddTab(GURL url, OnTaskBlocklist::RestrictionLevel restriction_level);

   private:
    // Callback triggered when the Boca SWA is launched. Normally at the onset
    // of a Boca session.
    void OnBocaSWALaunched(bool success);

    // Owned by the parent class `OnTaskSessionManager` that owns an instance of
    // the class `SystemWebAppLaunchHelper`, so there won't be UAF errors.
    raw_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;

    SEQUENCE_CHECKER(sequence_checker_);

    bool launch_in_progress_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

    base::WeakPtrFactory<SystemWebAppLaunchHelper> weak_ptr_factory_{this};
  };

  const std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;

  const std::unique_ptr<SystemWebAppLaunchHelper> system_web_app_launch_helper_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
