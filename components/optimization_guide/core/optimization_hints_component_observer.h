// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_HINTS_COMPONENT_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_HINTS_COMPONENT_OBSERVER_H_

namespace optimization_guide {

struct HintsComponentInfo;

// Interface for objects that wish to be notified of changes to the Optimization
// Hints Component.
//
// All calls will be made on the UI thread.
class OptimizationHintsComponentObserver {
 public:
  // Called when a new hints component is available for processing. While this
  // is called on the UI thread, it is recommended that processing of the new
  // component occur on a background thread.
  virtual void OnHintsComponentAvailable(const HintsComponentInfo& info) = 0;

 protected:
  virtual ~OptimizationHintsComponentObserver() = default;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_HINTS_COMPONENT_OBSERVER_H_
