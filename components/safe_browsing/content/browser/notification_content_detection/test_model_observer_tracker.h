// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_TEST_MODEL_OBSERVER_TRACKER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_TEST_MODEL_OBSERVER_TRACKER_H_

#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"

namespace safe_browsing {

class TestModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  TestModelObserverTracker();

  ~TestModelObserverTracker() override;

  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override;

  bool DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget target,
      std::optional<optimization_guide::proto::Any>* out_model_metadata) const;

 private:
  base::flat_map<optimization_guide::proto::OptimizationTarget,
                 std::optional<optimization_guide::proto::Any>>
      registered_model_metadata_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_TEST_MODEL_OBSERVER_TRACKER_H_
