// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_THROTTLE_MANAGER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_THROTTLE_MANAGER_H_

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace data_reduction_proxy {

class DataReductionProxyServer;
class DataReductionProxyThrottleManager;

// A throttle config observer that is additionally notified about the manager's
// destruction.
class DataReductionProxyThrottleConfigCheckedObserver
    : public mojom::DataReductionProxyThrottleConfigObserver,
      public base::CheckedObserver {
 public:
  virtual void OnThrottleManagerDestroyed(
      DataReductionProxyThrottleManager* manager) = 0;
};

// Helper that encapsulates the shared state between
// DataReductionProxyURLThrottles, whose main responsibility is keeping the
// shared mojo connections required by the throttles.
class DataReductionProxyThrottleManager
    : public mojom::DataReductionProxyThrottleConfigObserver {
 public:
  // Observes |data_reduction_proxy| for changes to the config, and starts
  // off with the initial value (possibly empty) |initial_config|.
  DataReductionProxyThrottleManager(
      mojom::DataReductionProxy* data_reduction_proxy,
      mojom::DataReductionProxyThrottleConfigPtr initial_config);

  ~DataReductionProxyThrottleManager() override;

  // mojom::DataReductionProxyThrottleConfigObserver implementation.
  void OnThrottleConfigChanged(
      mojom::DataReductionProxyThrottleConfigPtr config) override;

  // Called by throttles living on the manager's sequence when they want to
  // sign up for / sign out of receiving
  // mojom::DataReductionProxyThrottleConfigObserver events.
  void AddSameSequenceObserver(
      DataReductionProxyThrottleConfigCheckedObserver* observer);
  void RemoveSameSequenceObserver(
      DataReductionProxyThrottleConfigCheckedObserver* observer);

  mojom::DataReductionProxy* data_reduction_proxy() {
    return shared_data_reduction_proxy_;
  }

  mojom::DataReductionProxyThrottleConfigPtr last_proxy_config() const {
    return last_proxy_config_.Clone();
  }

  static mojom::DataReductionProxyThrottleConfigPtr CreateConfig(
      const std::vector<DataReductionProxyServer>& proxies_for_http);

 private:
  // Most DataReductionProxyURLThrottles will live on the manager's sequence.
  // It makes sense for all of them to reuse the manager's mojo pipes set up
  // for the interfaces mojom::DataReductionProxy and
  // mojom::DataReductionProxyThrottleConfigObserver.
  mojom::DataReductionProxy* const shared_data_reduction_proxy_;
  base::ObserverList<DataReductionProxyThrottleConfigCheckedObserver,
                     /* check_empty = */ true>
      same_sequence_observers_;

  mojo::Receiver<
      data_reduction_proxy::mojom::DataReductionProxyThrottleConfigObserver>
      receiver_{this};

  // The last seen config values.
  mojom::DataReductionProxyThrottleConfigPtr last_proxy_config_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyThrottleManager);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_THROTTLE_MANAGER_H_
