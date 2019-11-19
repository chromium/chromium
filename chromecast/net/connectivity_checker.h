// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequenced_task_runner_helpers.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class SharedURLLoaderFactoryInfo;
class NetworkConnectionTracker;
}  // namespace network

namespace chromecast {

// Checks if internet connectivity is available.
class ConnectivityChecker
    : public base::RefCountedDeleteOnSequence<ConnectivityChecker> {
 public:
  class ConnectivityObserver {
   public:
    // Will be called when internet connectivity changes.
    virtual void OnConnectivityChanged(bool connected) = 0;

   protected:
    ConnectivityObserver() {}
    virtual ~ConnectivityObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectivityObserver);
  };

  static scoped_refptr<ConnectivityChecker> Create(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      network::NetworkConnectionTracker* network_connection_tracker);

  void AddConnectivityObserver(ConnectivityObserver* observer);
  void RemoveConnectivityObserver(ConnectivityObserver* observer);

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

 private:
  friend class base::RefCountedDeleteOnSequence<ConnectivityChecker>;
  friend class base::DeleteHelper<ConnectivityChecker>;

  const scoped_refptr<base::ObserverListThreadSafe<ConnectivityObserver>>
      connectivity_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ConnectivityChecker);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
