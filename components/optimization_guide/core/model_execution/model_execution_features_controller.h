// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

class PrefService;

namespace optimization_guide {

namespace internal {

// Class that keeps track of user opt-in settings, including the visibility of
// settings and the user's opt-in state.
class OptimizationGuideModelExecutionFeaturesController {
 public:
  // Must be created only for non-incognito browser contexts.
  explicit OptimizationGuideModelExecutionFeaturesController(
      PrefService* browser_context_profile_service);

  OptimizationGuideModelExecutionFeaturesController(
      const OptimizationGuideModelExecutionFeaturesController&) = delete;
  OptimizationGuideModelExecutionFeaturesController& operator=(
      const OptimizationGuideModelExecutionFeaturesController&) = delete;

  // Returns true if the opt-in setting should be shown for this profile for
  // given `feature`.
  bool IsSettingVisible(proto::ModelExecutionFeature feature) const;

  // Returns true if the opt-in setting has been enabled by the user for this
  // profile for given `feature`.
  bool IsSettingEnabled(proto::ModelExecutionFeature feature) const;

 private:
  raw_ptr<PrefService> browser_context_profile_service_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};
}  // namespace internal

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FEATURES_CONTROLLER_H_
