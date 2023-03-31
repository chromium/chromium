// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_H_

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

struct ModelExecutionResult;

using proto::SegmentId;

// Central class responsible for scheduling model execution. Determines which
// models are eligible for execution based on various criteria e.g. cached
// results, TTL etc. Invoked from multiple classes such as segment
// selector or periodic jobs.
class ModelExecutionScheduler {
 public:
  // An observer to be notified about the model execution and results.
  class Observer {
   public:
    // Called whenever a model execution completes.
    virtual void OnModelExecutionCompleted(SegmentId segment_id) = 0;
  };

  virtual ~ModelExecutionScheduler() = default;

  // Called whenever a new or updated model is available. Must be a valid
  // SegmentInfo with valid metadata and features.
  virtual void OnNewModelInfoReady(const proto::SegmentInfo& segment_info) = 0;

  // Central method to determine which all models to execute. Called in response
  // to segmentation requests from clients, or periodic tasks. Can be called
  // repeatedly. Loops through all the models and decides whether to execute a
  // model or not. A segment is considered eligible to run based on
  // - if it has no results, or
  // - if the results have expired or
  // - if the results weren't computed too recently, and |expired_only| is false
  // - if all the required signals have been collected for sufficient duration.
  virtual void RequestModelExecutionForEligibleSegments(bool expired_only) = 0;

  // Runs model execution for a particular segment.
  virtual void RequestModelExecution(
      const proto::SegmentInfo& segment_info) = 0;

  // Called after model execution completes. If the execution was successful,
  // saves the results to the DB, and notifies observers. If the execution was
  // unsuccessful, deletes the result from the DB.
  // TODO(shaktisahu): Do we want to store that failure reason in the DB
  // instead? We might treat different failures differently next time.
  virtual void OnModelExecutionCompleted(
      const proto::SegmentInfo& segment_info,
      std::unique_ptr<ModelExecutionResult> result) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_H_
