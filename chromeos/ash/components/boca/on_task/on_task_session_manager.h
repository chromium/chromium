// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"

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

 private:
  // Callback triggered when the Boca SWA is launched. Normally at the onset
  // of a Boca session.
  void OnBocaSWALaunched(bool success);

  const std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;

  base::WeakPtrFactory<OnTaskSessionManager> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SESSION_MANAGER_H_
