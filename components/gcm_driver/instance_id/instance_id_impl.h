// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_delayed_task_controller.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace gcm {
class GCMDriver;
class InstanceIDHandler;
}  // namespace gcm

namespace instance_id {

// InstanceID implementation for desktop and iOS.
class InstanceIDImpl : public InstanceID {
 public:
  InstanceIDImpl(const std::string& app_id, gcm::GCMDriver* gcm_driver);

  InstanceIDImpl(const InstanceIDImpl&) = delete;
  InstanceIDImpl& operator=(const InstanceIDImpl&) = delete;

  ~InstanceIDImpl() override;

  // InstanceID:
  void GetID(GetIDCallback callback) override;
  void GetCreationTime(GetCreationTimeCallback callback) override;
  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                base::TimeDelta time_to_live,
                std::set<Flags> flags,
                GetTokenCallback callback) override;
  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override;
  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override;
  void DeleteIDImpl(DeleteIDCallback callback) override;

 private:
  void EnsureIDGenerated();

  void OnGetTokenCompleted(GetTokenCallback callback,
                           const std::string& token,
                           gcm::GCMClient::Result result);
  void OnDeleteTokenCompleted(DeleteTokenCallback callback,
                              gcm::GCMClient::Result result);
  void OnDeleteIDCompleted(DeleteIDCallback callback,
                           gcm::GCMClient::Result result);
  void GetInstanceIDDataCompleted(const std::string& instance_id,
                                  const std::string& extra_data);

  void DoGetID(GetIDCallback callback);
  void DoGetCreationTime(GetCreationTimeCallback callback);
  void DoGetToken(const std::string& authorized_entity,
                  const std::string& scope,
                  base::TimeDelta time_to_live,
                  GetTokenCallback callback);
  void DoValidateToken(const std::string& authorized_entity,
                       const std::string& scope,
                       const std::string& token,
                       ValidateTokenCallback callback);
  void DoDeleteToken(const std::string& authorized_entity,
                     const std::string& scope,
                     DeleteTokenCallback callback);
  void DoDeleteID(DeleteIDCallback callback);

  gcm::InstanceIDHandler* Handler();

  // Asynchronously runs task once delayed_task_controller_ is ready.
  void RunWhenReady(base::OnceClosure task);

  gcm::GCMDelayedTaskController delayed_task_controller_;

  // The generated Instance ID.
  std::string id_;

  // The time when the Instance ID has been generated.
  base::Time creation_time_;

  base::WeakPtrFactory<InstanceIDImpl> weak_ptr_factory_{this};
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_
