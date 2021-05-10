// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CORE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_member.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyClient;
class CloudPolicyRefreshScheduler;
class CloudPolicyService;
class CloudPolicyStore;
class RemoteCommandsFactory;
class RemoteCommandsService;

// CloudPolicyCore glues together the ingredients that are essential for
// obtaining a fully-functional cloud policy system: CloudPolicyClient and
// CloudPolicyStore, which are responsible for fetching policy from the cloud
// and storing it locally, respectively, as well as a CloudPolicyService
// instance that moves data between the two former components, and
// CloudPolicyRefreshScheduler which triggers periodic refreshes.
class POLICY_EXPORT CloudPolicyCore {
 public:
  // Callbacks for policy core events.
  class POLICY_EXPORT Observer {
   public:
    virtual ~Observer();

    // Called after the core is connected.
    virtual void OnCoreConnected(CloudPolicyCore* core) = 0;

    // Called after the refresh scheduler is started.
    virtual void OnRefreshSchedulerStarted(CloudPolicyCore* core) = 0;

    // Called before the core is disconnected.
    virtual void OnCoreDisconnecting(CloudPolicyCore* core) = 0;

    // Called after the remote commands service is started. Defaults to be
    // empty.
    virtual void OnRemoteCommandsServiceStarted(CloudPolicyCore* core);
  };

  // |task_runner| is the runner for policy refresh tasks.
  CloudPolicyCore(const std::string& policy_type,
                  const std::string& settings_entity_id,
                  CloudPolicyStore* store,
                  const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                  network::NetworkConnectionTrackerGetter
                      network_connection_tracker_getter);
  ~CloudPolicyCore();

  CloudPolicyClient* client() { return client_.get(); }
  const CloudPolicyClient* client() const { return client_.get(); }

  CloudPolicyStore* store() { return store_; }
  const CloudPolicyStore* store() const { return store_; }

  CloudPolicyService* service() { return service_.get(); }
  const CloudPolicyService* service() const { return service_.get(); }

  CloudPolicyRefreshScheduler* refresh_scheduler() {
    return refresh_scheduler_.get();
  }
  const CloudPolicyRefreshScheduler* refresh_scheduler() const {
    return refresh_scheduler_.get();
  }

  RemoteCommandsService* remote_commands_service() {
    return remote_commands_service_.get();
  }
  const RemoteCommandsService* remote_commands_service() const {
    return remote_commands_service_.get();
  }

  // Initializes the cloud connection.
  void Connect(std::unique_ptr<CloudPolicyClient> client);

  // Shuts down the cloud connection.
  void Disconnect();

  // Starts a remote commands service, with the provided factory. Will attempt
  // to fetch commands immediately, thus requiring the cloud policy client to
  // be registered.
  void StartRemoteCommandsService(
      std::unique_ptr<RemoteCommandsFactory> factory,
      PolicyInvalidationScope scope);

  // Requests a policy refresh to be performed soon. This may apply throttling,
  // and the request may not be immediately sent.
  void RefreshSoon();

  // Starts a refresh scheduler in case none is running yet.
  void StartRefreshScheduler();

  // Watches the pref named |refresh_pref_name| in |pref_service| and adjusts
  // |refresh_scheduler_|'s refresh delay accordingly.
  void TrackRefreshDelayPref(PrefService* pref_service,
                             const std::string& refresh_pref_name);

  // Registers an observer to be notified of policy core events.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

  // Initializes the cloud connection using injected |service| and |client|.
  void ConnectForTesting(std::unique_ptr<CloudPolicyService> service,
                         std::unique_ptr<CloudPolicyClient> client);

 private:
  // Updates the refresh scheduler on refresh delay changes.
  void UpdateRefreshDelayFromPref();

  std::string policy_type_;
  std::string settings_entity_id_;
  CloudPolicyStore* store_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  network::NetworkConnectionTrackerGetter network_connection_tracker_getter_;
  std::unique_ptr<CloudPolicyClient> client_;
  std::unique_ptr<CloudPolicyService> service_;
  std::unique_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;
  std::unique_ptr<RemoteCommandsService> remote_commands_service_;
  std::unique_ptr<IntegerPrefMember> refresh_delay_;
  base::ObserverList<Observer, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCore);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CORE_H_
