// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"

namespace optimization_guide {

// Tracks the info for the current Optimization Hints component and notifies
// observers of newly available hints components downloaded from the Component
// Updater.
class OptimizationGuideService {
 public:
  explicit OptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread_task_runner);

  virtual ~OptimizationGuideService();

  // Adds the observer and synchronously dispatches the current
  // HintsComponentInfo to it if one is already available.
  //
  // Virtual so it can be mocked out in tests.
  virtual void AddObserver(OptimizationGuideServiceObserver* observer);
  // Virtual so it can be mocked out in tests.
  virtual void RemoveObserver(OptimizationGuideServiceObserver* observer);

  // Forwards the update hints component request on to the UI thread, where the
  // actual work occurs.
  //
  // Virtual so it can be mocked out in tests.
  virtual void MaybeUpdateHintsComponent(const HintsComponentInfo& info);

  // If the hints component version in |info| is greater than that in
  // |hints_component_info_|, updates |hints_component_info_| and dispatches it
  // to all observers. In the case where the version is not greater, it does
  // nothing.
  void MaybeUpdateHintsComponentOnUIThread(const HintsComponentInfo& info);

 private:
  // Runner for indexing tasks.
  SEQUENCE_CHECKER(sequence_checker_);

  // Runner for UI Thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  // Observers receiving notifications on hints being processed.
  base::ObserverList<OptimizationGuideServiceObserver>::Unchecked observers_;

  // The current HintsComponentInfo available to observers. This is unset until
  // the first time MaybeUpdateHintsComponent() is called.
  base::Optional<HintsComponentInfo> hints_component_info_;

  // Used to get |weak_ptr_| to self.
  base::WeakPtrFactory<OptimizationGuideService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideService);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
