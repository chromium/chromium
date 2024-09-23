// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_BASE_H_
#define COMPONENTS_POLICY_CORE_BROWSER_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/policy_export.h"

namespace invalidation {
class ProfileInvalidationProvider;
}

namespace policy {

class CloudPolicyCore;
class RemoteCommandsFactory;
class RemoteCommandsInvalidator;

// A `KeyedService` of UserRemoteCommandService which is responsible for
// initialize and maintain the ownership of a remote command service at the
// profile scope. More details can be found at b/276764782
// The base class contains all logic that can be shared with iOS in the future.
class POLICY_EXPORT UserRemoteCommandsServiceBase
    : public KeyedService,
      public CloudPolicyService::Observer {
 public:
  explicit UserRemoteCommandsServiceBase(CloudPolicyCore* core);
  UserRemoteCommandsServiceBase(const UserRemoteCommandsServiceBase&) = delete;
  UserRemoteCommandsServiceBase& operator=(
      const UserRemoteCommandsServiceBase&) = delete;
  ~UserRemoteCommandsServiceBase() override;

  void Init();

  // CloudPolicyService::Observer implementation:
  void OnCloudPolicyServiceInitializationCompleted() override;
  void OnPolicyRefreshed(bool success) override;
  std::string_view name() const override;

  // KeyedService
  void Shutdown() override;

 protected:
  virtual invalidation::ProfileInvalidationProvider*
  GetInvalidationProvider() = 0;

  virtual std::unique_ptr<RemoteCommandsFactory> GetFactory() = 0;

 private:
  raw_ptr<CloudPolicyCore> core_;
  base::ScopedObservation<CloudPolicyService, CloudPolicyService::Observer>
      cloud_policy_service_observer_{this};

  std::unique_ptr<RemoteCommandsInvalidator> invalidator_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_BASE_H_
