// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_TARGET_MODEL_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_TARGET_MODEL_OBSERVER_H_

#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Observes |optimization_guide::OptimizationGuideDecider| for updates to models
// for a particular optimization target.
class OptimizationTargetModelObserver : public base::CheckedObserver {
 public:
  // Invoked when a model for |optimization_target| has been updated. It is
  // guaranteed that this method will only be invoked for targets that |this|
  // is added as an observer for.
  //
  // When this observer is first added, it will call this function with the
  // model it already has on device, if applicable.
  virtual void OnModelUpdated(proto::OptimizationTarget optimization_target,
                              const ModelInfo& model_info) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_TARGET_MODEL_OBSERVER_H_
