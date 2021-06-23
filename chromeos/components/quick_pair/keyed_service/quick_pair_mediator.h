// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
#define CHROMEOS_COMPONENTS_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chromeos/components/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"

namespace chromeos {
namespace quick_pair {

// Implements the Mediator design pattern for the components in the Quick Pair
// system, e.g. the UI Broker, Scanning Broker and Pairing Broker.
class Mediator : public FeatureStatusTracker::Observer {
 public:
  explicit Mediator(
      std::unique_ptr<FeatureStatusTracker> feature_status_tracker);
  Mediator(const Mediator&) = delete;
  Mediator& operator=(const Mediator&) = delete;
  ~Mediator() final;

  // QuickPairFeatureStatusTracker::Observer
  void OnFastPairEnabledChanged(bool is_enabled) override;

 private:
  void SetFastPairState(bool is_enabled);

  std::unique_ptr<FeatureStatusTracker> feature_status_tracker_;
  base::ScopedObservation<FeatureStatusTracker, FeatureStatusTracker::Observer>
      observation_{this};
};

}  // namespace quick_pair
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_MEDIATOR_H_
