// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"

#include "base/bit_cast.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#define CALL_MEMBER_FN(obj, func) ((obj).*(func))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x)[0])

using ukm::builders::Segmentation_ModelExecution;

namespace {
using UkmMemberFn =
    Segmentation_ModelExecution& (Segmentation_ModelExecution::*)(int64_t);

const UkmMemberFn kSegmentationUkmMethods[] = {
    &Segmentation_ModelExecution::SetInput0,
    &Segmentation_ModelExecution::SetInput1,
    &Segmentation_ModelExecution::SetInput2,
    &Segmentation_ModelExecution::SetInput3,
    &Segmentation_ModelExecution::SetInput4,
    &Segmentation_ModelExecution::SetInput5,
    &Segmentation_ModelExecution::SetInput6,
    &Segmentation_ModelExecution::SetInput7,
    &Segmentation_ModelExecution::SetInput8,
    &Segmentation_ModelExecution::SetInput9,
    &Segmentation_ModelExecution::SetInput10,
    &Segmentation_ModelExecution::SetInput11,
    &Segmentation_ModelExecution::SetInput12,
    &Segmentation_ModelExecution::SetInput13,
    &Segmentation_ModelExecution::SetInput14,
    &Segmentation_ModelExecution::SetInput15,
    &Segmentation_ModelExecution::SetInput16,
    &Segmentation_ModelExecution::SetInput17,
    &Segmentation_ModelExecution::SetInput18,
    &Segmentation_ModelExecution::SetInput19,
    &Segmentation_ModelExecution::SetInput20,
    &Segmentation_ModelExecution::SetInput21,
    &Segmentation_ModelExecution::SetInput22,
    &Segmentation_ModelExecution::SetInput23,
    &Segmentation_ModelExecution::SetInput24,
    &Segmentation_ModelExecution::SetInput25,
    &Segmentation_ModelExecution::SetInput26,
    &Segmentation_ModelExecution::SetInput27,
    &Segmentation_ModelExecution::SetInput28,
    &Segmentation_ModelExecution::SetInput29};

// Gets a set of segment IDs that are allowed to upload metrics.
base::flat_set<int> GetSegmentIdsAllowedForReporting() {
  std::vector<std::string> segment_ids = base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          segmentation_platform::features::
              kSegmentationStructuredMetricsFeature,
          segmentation_platform::kSegmentIdsAllowedForReportingKey),
      ",;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  base::flat_set<int> result;
  for (const auto& id : segment_ids) {
    int segment_id;
    if (base::StringToInt(id, &segment_id))
      result.emplace(segment_id);
  }
  return result;
}

}  // namespace

namespace segmentation_platform {

SegmentationUkmHelper::SegmentationUkmHelper() {
  Initialize();
}

SegmentationUkmHelper::~SegmentationUkmHelper() = default;

void SegmentationUkmHelper::Initialize() {
  allowed_segment_ids_ = GetSegmentIdsAllowedForReporting();
}

// static
SegmentationUkmHelper* SegmentationUkmHelper::GetInstance() {
  static base::NoDestructor<SegmentationUkmHelper> helper;
  return helper.get();
}

ukm::SourceId SegmentationUkmHelper::RecordModelExecutionResult(
    OptimizationTarget segment_id,
    int64_t model_version,
    const std::vector<float>& input_tensor,
    float result) {
  // Check if the |segment_id| is allowed to record metrics.
  if (!allowed_segment_ids_.contains(static_cast<int>(segment_id)))
    return ukm::kInvalidSourceId;

  ukm::SourceId source_id = ukm::NoURLSourceId();
  // Don't record UKM if there are too many tensors.
  if (input_tensor.size() > ARRAY_SIZE(kSegmentationUkmMethods)) {
    stats::RecordTooManyInputTensors(input_tensor.size());
    return ukm::kInvalidSourceId;
  }
  ukm::builders::Segmentation_ModelExecution execution_result(source_id);

  execution_result.SetOptimizationTarget(segment_id)
      .SetModelVersion(model_version);

  for (size_t i = 0; i < input_tensor.size(); ++i) {
    CALL_MEMBER_FN(execution_result, kSegmentationUkmMethods[i])
    (FloatToInt64(input_tensor[i]));
  }
  execution_result.SetPredictionResult(FloatToInt64(result))
      .Record(ukm::UkmRecorder::Get());
  return source_id;
}

// static
int64_t SegmentationUkmHelper::FloatToInt64(float f) {
  // Encode the float number in IEEE754 double precision.
  return bit_cast<int64_t>(static_cast<double>(f));
}

}  // namespace segmentation_platform
