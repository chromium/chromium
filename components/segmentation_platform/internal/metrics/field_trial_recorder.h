// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METRICS_FIELD_TRIAL_RECORDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METRICS_FIELD_TRIAL_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

struct Config;
class FieldTrialRegister;

// Records field trials for every client.
class FieldTrialRecorder {
 public:
  explicit FieldTrialRecorder(FieldTrialRegister* field_trial_register);
  ~FieldTrialRecorder();

  FieldTrialRecorder(const FieldTrialRecorder&) = delete;
  FieldTrialRecorder& operator=(const FieldTrialRecorder&) = delete;

  // Records field trial for all client based on their `OutputConfig`. It gets
  // the result from last session for the client using `CachedResultProvider`.
  // Based on these results, record the field trial. Currently it supports field
  // trial recording the top most output label for the following.
  //  1. BinnedClassifier
  //  2. BinaryClassifier
  //  3. MultiClassClassifier with `top_k_outputs` as 1. (We only record top
  //  label as the field trial sice we possibly can't create all permutations of
  //  all the labels.)
  void RecordFieldTrialAtStartup(
      const std::vector<std::unique_ptr<Config>>& configs,
      CachedResultProvider* cached_result_provider);

 private:
  const raw_ptr<FieldTrialRegister> field_trial_register_;

  base::WeakPtrFactory<FieldTrialRecorder> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METRICS_FIELD_TRIAL_RECORDER_H_
