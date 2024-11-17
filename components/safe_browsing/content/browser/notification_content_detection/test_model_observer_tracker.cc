// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"

namespace safe_browsing {

TestModelObserverTracker::TestModelObserverTracker() = default;
TestModelObserverTracker::~TestModelObserverTracker() = default;

void TestModelObserverTracker::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget target,
    const std::optional<optimization_guide::proto::Any>& model_metadata,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  registered_model_metadata_.insert_or_assign(target, model_metadata);
}

bool TestModelObserverTracker::DidRegisterForTarget(
    optimization_guide::proto::OptimizationTarget target,
    std::optional<optimization_guide::proto::Any>* out_model_metadata) const {
  auto it = registered_model_metadata_.find(target);
  if (it == registered_model_metadata_.end()) {
    return false;
  }
  *out_model_metadata = registered_model_metadata_.at(target);
  return true;
}

}  // namespace safe_browsing
