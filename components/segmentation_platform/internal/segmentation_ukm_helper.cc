// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"

#include "base/bit_cast.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#define CALL_MEMBER_FN(obj, func) ((obj).*(func))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x)[0])

using segmentation_platform::proto::SegmentId;
using ukm::builders::Segmentation_ModelExecution;

namespace {
using UkmMemberFn =
    Segmentation_ModelExecution& (Segmentation_ModelExecution::*)(int64_t);

const UkmMemberFn kSegmentationUkmInputMethods[] = {
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
    &Segmentation_ModelExecution::SetInput29,
    &Segmentation_ModelExecution::SetInput30,
    &Segmentation_ModelExecution::SetInput31,
    &Segmentation_ModelExecution::SetInput32,
    &Segmentation_ModelExecution::SetInput33,
    &Segmentation_ModelExecution::SetInput34,
    &Segmentation_ModelExecution::SetInput35,
    &Segmentation_ModelExecution::SetInput36,
    &Segmentation_ModelExecution::SetInput37,
    &Segmentation_ModelExecution::SetInput38,
    &Segmentation_ModelExecution::SetInput39,
    &Segmentation_ModelExecution::SetInput40,
    &Segmentation_ModelExecution::SetInput41,
    &Segmentation_ModelExecution::SetInput42,
    &Segmentation_ModelExecution::SetInput43,
    &Segmentation_ModelExecution::SetInput44,
    &Segmentation_ModelExecution::SetInput45,
    &Segmentation_ModelExecution::SetInput46,
    &Segmentation_ModelExecution::SetInput47,
    &Segmentation_ModelExecution::SetInput48,
    &Segmentation_ModelExecution::SetInput49};

const UkmMemberFn kSegmentationUkmOutputMethods[] = {
    &Segmentation_ModelExecution::SetActualResult,
    &Segmentation_ModelExecution::SetActualResult2,
    &Segmentation_ModelExecution::SetActualResult3,
    &Segmentation_ModelExecution::SetActualResult4,
    &Segmentation_ModelExecution::SetActualResult5,
    &Segmentation_ModelExecution::SetActualResult6};

base::flat_set<SegmentId> GetSegmentIdsAllowedForReporting() {
  // TODO(crbug.com/1406404): Get allowed segment Ids from database.
  if (base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationDefaultReportingSegments)) {
    return std::vector<SegmentId>{
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT};
  }
  std::vector<std::string> segment_ids = base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          segmentation_platform::features::
              kSegmentationStructuredMetricsFeature,
          segmentation_platform::kSegmentIdsAllowedForReportingKey),
      ",;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  base::flat_set<SegmentId> result;
  for (const auto& id : segment_ids) {
    int segment_id;
    if (base::StringToInt(id, &segment_id))
      result.emplace(static_cast<SegmentId>(segment_id));
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
    SegmentId segment_id,
    int64_t model_version,
    const ModelProvider::Request& input_tensor,
    float result) {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::Segmentation_ModelExecution execution_result(source_id);

  // Add inputs to ukm message.
  if (!AddInputsToUkm(&execution_result, segment_id, model_version,
                      input_tensor)) {
    return ukm::kInvalidSourceId;
  }

  // TODO(xingliu): Also record continuous outputs for model execution.
  execution_result.SetPredictionResult(FloatToInt64(result))
      .Record(ukm::UkmRecorder::Get());
  return source_id;
}

ukm::SourceId SegmentationUkmHelper::RecordTrainingData(
    SegmentId segment_id,
    int64_t model_version,
    const ModelProvider::Request& input_tensor,
    const ModelProvider::Response& outputs,
    const std::vector<int>& output_indexes,
    absl::optional<proto::PredictionResult> prediction_result,
    absl::optional<SelectedSegment> selected_segment) {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::Segmentation_ModelExecution execution_result(source_id);
  if (!AddInputsToUkm(&execution_result, segment_id, model_version,
                      input_tensor)) {
    return ukm::kInvalidSourceId;
  }

  if (!AddOutputsToUkm(&execution_result, outputs, output_indexes)) {
    return ukm::kInvalidSourceId;
  }

  if (prediction_result.has_value() && prediction_result->result_size() > 0) {
    // TODO(ritikagup): Add support for uploading multiple outputs.
    execution_result.SetPredictionResult(
        FloatToInt64(prediction_result->result()[0]));
  }
  if (selected_segment.has_value()) {
    execution_result.SetSelectionResult(selected_segment->segment_id);
    execution_result.SetOutputDelaySec(
        (base::Time::Now() - selected_segment->selection_time).InSeconds());
  }

  execution_result.Record(ukm::UkmRecorder::Get());
  return source_id;
}

bool SegmentationUkmHelper::AddInputsToUkm(
    ukm::builders::Segmentation_ModelExecution* ukm_builder,
    SegmentId segment_id,
    int64_t model_version,
    const ModelProvider::Request& input_tensor) {
  if (input_tensor.size() > ARRAY_SIZE(kSegmentationUkmInputMethods)) {
    // Don't record UKM if there are too many tensors.
    stats::RecordTooManyInputTensors(input_tensor.size());
    return false;
  }

  ukm_builder->SetOptimizationTarget(segment_id).SetModelVersion(model_version);
  for (size_t i = 0; i < input_tensor.size(); ++i) {
    CALL_MEMBER_FN(*ukm_builder, kSegmentationUkmInputMethods[i])
    (FloatToInt64(input_tensor[i]));
  }
  return true;
}

bool SegmentationUkmHelper::AddOutputsToUkm(
    ukm::builders::Segmentation_ModelExecution* ukm_builder,
    const ModelProvider::Response& outputs,
    const std::vector<int>& output_indexes) {
  DCHECK(!outputs.empty());
  if (outputs.size() != output_indexes.size())
    return false;

  const int output_methods_size = ARRAY_SIZE(kSegmentationUkmOutputMethods);
  if (outputs.size() > output_methods_size)
    return false;

  for (size_t i = 0; i < outputs.size(); ++i) {
    if (output_indexes[i] >= output_methods_size)
      return false;
    CALL_MEMBER_FN(*ukm_builder,
                   kSegmentationUkmOutputMethods[output_indexes[i]])
    (FloatToInt64(outputs[i]));
  }

  return true;
}

bool SegmentationUkmHelper::CanUploadTensors(
    const proto::SegmentInfo& segment_info) const {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationStructuredMetricsFeature)) {
    return false;
  }
  return segment_info.model_metadata().upload_tensors() ||
         allowed_segment_ids_.contains(
             static_cast<int>(segment_info.segment_id()));
}

// static
int64_t SegmentationUkmHelper::FloatToInt64(float f) {
  // Encode the float number in IEEE754 double precision.
  return base::bit_cast<int64_t>(static_cast<double>(f));
}

// static
bool SegmentationUkmHelper::AllowedToUploadData(
    base::TimeDelta signal_storage_length,
    base::Clock* clock) {
  base::Time most_recent_allowed = LocalStateHelper::GetInstance().GetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey);
  // If the local state is never set, return false.
  if (most_recent_allowed.is_null() ||
      most_recent_allowed == base::Time::Max()) {
    return false;
  }
  return most_recent_allowed + signal_storage_length < clock->Now();
}

}  // namespace segmentation_platform
