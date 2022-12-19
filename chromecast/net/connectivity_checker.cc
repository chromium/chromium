// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker.h"

#include "base/task/single_thread_task_runner.h"
#include "chromecast/net/connectivity_checker_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromecast {

ConnectivityChecker::ConnectivityChecker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : RefCountedDeleteOnSequence(std::move(task_runner)),
      connectivity_observer_list_(
          base::MakeRefCounted<
              base::ObserverListThreadSafe<ConnectivityObserver>>()),
      connectivity_check_failure_observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<
              ConnectivityCheckFailureObserver>>()) {}

ConnectivityChecker::~ConnectivityChecker() = default;

void ConnectivityChecker::AddConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_->AddObserver(observer);
}

void ConnectivityChecker::RemoveConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_->RemoveObserver(observer);
}

void ConnectivityChecker::Notify(bool connected) {
  DCHECK(connectivity_observer_list_.get());
  connectivity_observer_list_->Notify(
      FROM_HERE, &ConnectivityObserver::OnConnectivityChanged, connected);
}

void ConnectivityChecker::AddConnectivityCheckFailureObserver(
    ConnectivityCheckFailureObserver* observer) {
  connectivity_check_failure_observer_list_->AddObserver(observer);
}

void ConnectivityChecker::RemoveConnectivityCheckFailureObserver(
    ConnectivityCheckFailureObserver* observer) {
  connectivity_check_failure_observer_list_->RemoveObserver(observer);
}

void ConnectivityChecker::NotifyCheckFailure() {
  DCHECK(connectivity_check_failure_observer_list_.get());
  connectivity_check_failure_observer_list_->Notify(
      FROM_HERE, &ConnectivityCheckFailureObserver::OnConnectivityCheckFailed);
}

// static
scoped_refptr<ConnectivityChecker> ConnectivityChecker::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    TimeSyncTracker* time_sync_tracker) {
  return ConnectivityCheckerImpl::Create(
      task_runner, std::move(pending_url_loader_factory),
      network_connection_tracker, time_sync_tracker);
}

// static
scoped_refptr<ConnectivityChecker> ConnectivityChecker::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    base::TimeDelta disconnected_probe_period,
    base::TimeDelta connected_probe_period,
    TimeSyncTracker* time_sync_tracker) {
  return ConnectivityCheckerImpl::Create(
      task_runner, std::move(pending_url_loader_factory),
      network_connection_tracker, disconnected_probe_period,
      connected_probe_period, time_sync_tracker);
}

}  // namespace chromecast
