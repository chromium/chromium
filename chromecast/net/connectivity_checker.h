// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_H_

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/observer_list_threadsafe.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "chromecast/net/time_sync_tracker.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class PendingSharedURLLoaderFactory;
class NetworkConnectionTracker;
}  // namespace network

namespace chromecast {

// Checks if internet connectivity is available.
class ConnectivityChecker
    : public base::RefCountedDeleteOnSequence<ConnectivityChecker> {
 public:
  class ConnectivityObserver {
   public:
    ConnectivityObserver(const ConnectivityObserver&) = delete;
    ConnectivityObserver& operator=(const ConnectivityObserver&) = delete;

    // Will be called when internet connectivity changes.
    virtual void OnConnectivityChanged(bool connected) = 0;

   protected:
    ConnectivityObserver() {}
    virtual ~ConnectivityObserver() {}
  };

  class ConnectivityCheckFailureObserver {
   public:
    ConnectivityCheckFailureObserver(const ConnectivityCheckFailureObserver&) =
        delete;
    ConnectivityCheckFailureObserver& operator=(
        const ConnectivityCheckFailureObserver&) = delete;

    // will be called when connectivity check failed.
    virtual void OnConnectivityCheckFailed() = 0;

   protected:
    ConnectivityCheckFailureObserver() = default;
    virtual ~ConnectivityCheckFailureObserver() = default;
  };

  static scoped_refptr<ConnectivityChecker> Create(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      TimeSyncTracker* time_sync_tracker = nullptr);

  // Static factory with additional parameters for connectivity check period
  // - disconnected_probe_period:
  //       connectivity check period while disconnected.
  // - connected_probe_period:
  //       connectivity check period while connected.
  static scoped_refptr<ConnectivityChecker> Create(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      base::TimeDelta disconnected_probe_period,
      base::TimeDelta connected_probe_period,
      TimeSyncTracker* time_sync_tracker = nullptr);

  ConnectivityChecker(const ConnectivityChecker&) = delete;
  ConnectivityChecker& operator=(const ConnectivityChecker&) = delete;

  void AddConnectivityObserver(ConnectivityObserver* observer);
  void RemoveConnectivityObserver(ConnectivityObserver* observer);

  void AddConnectivityCheckFailureObserver(
      ConnectivityCheckFailureObserver* observer);
  void RemoveConnectivityCheckFailureObserver(
      ConnectivityCheckFailureObserver* observer);

  // Returns if there is internet connectivity.
  virtual bool Connected() const = 0;

  // Checks for connectivity.
  virtual void Check() = 0;

 protected:
  explicit ConnectivityChecker(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~ConnectivityChecker();

  // Notifies observes that connectivity has changed.
  void Notify(bool connected);

  // Notifies observers that connectivity check failed.
  void NotifyCheckFailure();

 private:
  friend class base::RefCountedDeleteOnSequence<ConnectivityChecker>;
  friend class base::DeleteHelper<ConnectivityChecker>;

  const scoped_refptr<base::ObserverListThreadSafe<ConnectivityObserver>>
      connectivity_observer_list_;
  const scoped_refptr<
      base::ObserverListThreadSafe<ConnectivityCheckFailureObserver>>
      connectivity_check_failure_observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
