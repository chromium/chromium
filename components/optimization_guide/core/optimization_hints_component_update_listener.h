// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_HINTS_COMPONENT_UPDATE_LISTENER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_HINTS_COMPONENT_UPDATE_LISTENER_H_

#include <optional>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/optimization_hints_component_observer.h"

class OptimizationGuideServiceTest;

namespace optimization_guide {

// Tracks the info for the current Optimization Hints component and notifies
// observers of newly available hints components downloaded from the Component
// Updater.
class OptimizationHintsComponentUpdateListener {
 public:
  static OptimizationHintsComponentUpdateListener* GetInstance();

  OptimizationHintsComponentUpdateListener(
      const OptimizationHintsComponentUpdateListener&) = delete;
  OptimizationHintsComponentUpdateListener& operator=(
      const OptimizationHintsComponentUpdateListener&) = delete;

  // Adds the observer and synchronously dispatches the current
  // HintsComponentInfo to it if one is already available.
  //
  // Virtual so it can be mocked out in tests.
  virtual void AddObserver(OptimizationHintsComponentObserver* observer);
  // Virtual so it can be mocked out in tests.
  virtual void RemoveObserver(OptimizationHintsComponentObserver* observer);

  // If the hints component version in |info| is greater than that in
  // |hints_component_info_|, updates |hints_component_info_| and dispatches it
  // to all observers. In the case where the version is not greater, it does
  // nothing.
  void MaybeUpdateHintsComponent(const HintsComponentInfo& info);

  // Currently received HintsComponentInfo.
  std::optional<HintsComponentInfo> hints_component_info() {
    return hints_component_info_;
  }

 private:
  OptimizationHintsComponentUpdateListener();
  ~OptimizationHintsComponentUpdateListener();

  friend class base::NoDestructor<OptimizationHintsComponentUpdateListener>;
  friend class OptimizationHintsComponentUpdateListenerTest;
  friend class ::OptimizationGuideServiceTest;

  void ResetStateForTesting();

  // Runner for indexing tasks.
  SEQUENCE_CHECKER(sequence_checker_);

  // Observers receiving notifications on hints being processed.
  base::ObserverList<OptimizationHintsComponentObserver>::Unchecked observers_;

  // The current HintsComponentInfo available to observers. This is unset until
  // the first time MaybeUpdateHintsComponent() is called.
  std::optional<HintsComponentInfo> hints_component_info_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_HINTS_COMPONENT_UPDATE_LISTENER_H_
