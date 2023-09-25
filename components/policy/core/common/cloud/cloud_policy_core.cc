// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_core.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/prefs/pref_service.h"

namespace policy {

CloudPolicyCore::Observer::~Observer() = default;

void CloudPolicyCore::Observer::OnRemoteCommandsServiceStarted(
    CloudPolicyCore* core) {}

void CloudPolicyCore::Observer::OnCoreDestruction(CloudPolicyCore* core) {}

CloudPolicyCore::CloudPolicyCore(
    const std::string& policy_type,
    const std::string& settings_entity_id,
    CloudPolicyStore* store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : policy_type_(policy_type),
      settings_entity_id_(settings_entity_id),
      store_(store),
      task_runner_(task_runner),
      network_connection_tracker_getter_(
          std::move(network_connection_tracker_getter)) {}

CloudPolicyCore::~CloudPolicyCore() {
  Disconnect();
  for (auto& observer : observers_)
    observer.OnCoreDestruction(this);
}

void CloudPolicyCore::Connect(std::unique_ptr<CloudPolicyClient> client) {
  CHECK(!client_);
  CHECK(client);
  client_ = std::move(client);
  service_ = std::make_unique<CloudPolicyService>(
      policy_type_, settings_entity_id_, client_.get(), store_);
  for (auto& observer : observers_)
    observer.OnCoreConnected(this);
}

void CloudPolicyCore::Disconnect() {
  if (client_)
    for (auto& observer : observers_)
      observer.OnCoreDisconnecting(this);
  refresh_delay_.reset();
  refresh_scheduler_.reset();
  remote_commands_service_.reset();
  service_.reset();
  client_.reset();
}

void CloudPolicyCore::StartRemoteCommandsService(
    std::unique_ptr<RemoteCommandsFactory> factory,
    PolicyInvalidationScope scope) {
  DCHECK(client_);
  DCHECK(factory);

  remote_commands_service_ = std::make_unique<RemoteCommandsService>(
      std::move(factory), client_.get(), store_, scope);

  // Do an initial remote commands fetch immediately.
  remote_commands_service_->FetchRemoteCommands();

  for (auto& observer : observers_)
    observer.OnRemoteCommandsServiceStarted(this);
}

void CloudPolicyCore::RefreshSoon(PolicyFetchReason reason) {
  if (refresh_scheduler_) {
    refresh_scheduler_->RefreshSoon(reason);
  }
}

void CloudPolicyCore::StartRefreshScheduler() {
  if (!refresh_scheduler_) {
    refresh_scheduler_ = std::make_unique<CloudPolicyRefreshScheduler>(
        client_.get(), store_, service_.get(), task_runner_,
        network_connection_tracker_getter_);
    UpdateRefreshDelayFromPref();
    for (auto& observer : observers_)
      observer.OnRefreshSchedulerStarted(this);
  }
}

void CloudPolicyCore::TrackRefreshDelayPref(
    PrefService* pref_service,
    const std::string& refresh_pref_name) {
  refresh_delay_ = std::make_unique<IntegerPrefMember>();
  refresh_delay_->Init(
      refresh_pref_name, pref_service,
      base::BindRepeating(&CloudPolicyCore::UpdateRefreshDelayFromPref,
                          base::Unretained(this)));
  UpdateRefreshDelayFromPref();
}

void CloudPolicyCore::AddObserver(CloudPolicyCore::Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyCore::RemoveObserver(CloudPolicyCore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyCore::ConnectForTesting(
    std::unique_ptr<CloudPolicyService> service,
    std::unique_ptr<CloudPolicyClient> client) {
  service_ = std::move(service);
  client_ = std::move(client);
  for (auto& observer : observers_)
    observer.OnCoreConnected(this);
}

void CloudPolicyCore::UpdateRefreshDelayFromPref() {
  if (refresh_scheduler_ && refresh_delay_)
    refresh_scheduler_->SetDesiredRefreshDelay(refresh_delay_->GetValue());
}

}  // namespace policy
