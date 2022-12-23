// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_POLICY_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_POLICY_OBSERVER_H_

#include <string>

namespace ash {

class NetworkPolicyObserver {
 public:
  NetworkPolicyObserver& operator=(const NetworkPolicyObserver&) = delete;

  // Called when the policies for |userhash| were set (also when they were
  // updated). An empty |userhash| designates the device policy.
  // Note that the policies might be empty and might not have been applied yet
  // at that time.
  virtual void PoliciesChanged(const std::string& userhash) {}

  // Called every time a policy application for |userhash| finished. This is
  // only called once no more policies are pending for |userhash|.
  // Because cellular policy application can be slow, the notification can be
  // dispatched even if cellular policy application is still in progress. In
  // this case, another |PoliciesApplied| notification will be invoked when
  // cellular policy application is done.
  virtual void PoliciesApplied(const std::string& userhash) {}

  // Called every time a network is created or updated because of a policy.
  virtual void PolicyAppliedToNetwork(const std::string& service_path) {}

  // Called just before ManagedNetworkConfigurationHandler is destroyed so that
  // observers can safely stop observing.
  virtual void OnManagedNetworkConfigurationHandlerShuttingDown() {}

 protected:
  virtual ~NetworkPolicyObserver() {}
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_POLICY_OBSERVER_H_
