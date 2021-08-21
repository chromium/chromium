// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_POLICY_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_POLICY_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class CellularESimInstaller;
enum class HermesResponseStatus;

// Handles provisioning eSIM profiles via policy.
//
// When installing policy eSIM profiles, the activation code is constructed from
// the SMDP address in the policy configuration. Install requests are queued and
// installation is performed one by one. Install attempts are retried for fixed
// number of tries.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularPolicyHandler {
 public:
  CellularPolicyHandler();
  CellularPolicyHandler(const CellularPolicyHandler&) = delete;
  CellularPolicyHandler& operator=(const CellularPolicyHandler&) = delete;
  ~CellularPolicyHandler();

  void Init(CellularESimInstaller* cellular_esim_installer);

  // Installs an ESim profile and connects to its network from policy with
  // given |smdp_address|. If another eSim profile is already under installation
  // process, the current request will wait until the previous one is completed.
  // Each installation will be retried for a fixed number of tries.
  void InstallESim(const std::string& smdp_address);

 private:
  friend class CellularPolicyHandlerTest;

  void ProcessRequests();
  void AttemptInstallESim();
  const std::string& GetCurrentSmdpAddress() const;
  void OnESimProfileInstallAttemptComplete(
      HermesResponseStatus hermes_status,
      absl::optional<dbus::ObjectPath> profile_path,
      absl::optional<std::string> service_path);
  void CompleteCurrentRequest(absl::optional<const std::string> iccid);

  CellularESimInstaller* cellular_esim_installer_ = nullptr;

  bool is_installing_ = false;
  base::circular_deque<std::string> remaining_install_requests_;

  // Provides us the backoff timers for AttemptInstallESim().
  net::BackoffEntry retry_backoff_;

  base::WeakPtrFactory<CellularPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_POLICY_HANDLER_H_