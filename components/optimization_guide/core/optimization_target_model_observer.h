// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_TARGET_MODEL_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_TARGET_MODEL_OBSERVER_H_

#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Observes |optimization_guide::OptimizationGuideDecider| for updates to models
// for a particular optimization target.
class OptimizationTargetModelObserver : public base::CheckedObserver {
 public:
  // Invoked when a model for |optimization_target| has been updated. It is
  // guaranteed that this method will only be invoked for targets that |this|
  // is added as an observer for. |model_metadata| will contain metadata that
  // the server has attached to this model, if applicable.
  //
  // When this observer is first added, it will call this function with the
  // file path it already has on device, if applicable.
  virtual void OnModelFileUpdated(
      proto::OptimizationTarget optimization_target,
      const absl::optional<proto::Any>& model_metadata,
      const base::FilePath& file_path) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_TARGET_MODEL_OBSERVER_H_
