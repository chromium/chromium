// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
namespace metadata_utils {

namespace {
uint64_t GetExpectedTensorLength(const proto::Feature& feature) {
  switch (feature.aggregation()) {
    case proto::Aggregation::COUNT:
    case proto::Aggregation::COUNT_BOOLEAN:
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT:
    case proto::Aggregation::SUM:
    case proto::Aggregation::SUM_BOOLEAN:
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT:
      return feature.bucket_count() == 0 ? 0 : 1;
    case proto::Aggregation::BUCKETED_COUNT:
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN:
    case proto::Aggregation::BUCKETED_CUMULATIVE_COUNT:
    case proto::Aggregation::BUCKETED_SUM:
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN:
    case proto::Aggregation::BUCKETED_CUMULATIVE_SUM:
      return feature.bucket_count();
    case proto::Aggregation::UNKNOWN:
      NOTREACHED();
      return 0;
  }
}
}  // namespace

ValidationResult ValidateSegmentInfo(const proto::SegmentInfo& segment_info) {
  if (segment_info.segment_id() ==
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN) {
    return ValidationResult::kSegmentIDNotFound;
  }

  if (!segment_info.has_model_metadata())
    return ValidationResult::kMetadataNotFound;

  return ValidateMetadata(segment_info.model_metadata());
}

ValidationResult ValidateMetadata(
    const proto::SegmentationModelMetadata& model_metadata) {
  if (model_metadata.time_unit() == proto::TimeUnit::UNKNOWN_TIME_UNIT)
    return ValidationResult::kTimeUnitInvald;

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataFeature(const proto::Feature& feature) {
  if (feature.type() == proto::SignalType::UNKNOWN_SIGNAL_TYPE)
    return ValidationResult::kSignalTypeInvalid;

  if ((feature.type() == proto::SignalType::HISTOGRAM_ENUM ||
       feature.type() == proto::SignalType::HISTOGRAM_VALUE) &&
      feature.name().empty()) {
    return ValidationResult::kFeatureNameNotFound;
  }

  if (feature.name_hash() == 0)
    return ValidationResult::kFeatureNameHashNotFound;

  if (!feature.name().empty() &&
      base::HashMetricName(feature.name()) != feature.name_hash()) {
    return ValidationResult::kFeatureNameHashDoesNotMatchName;
  }

  if (feature.aggregation() == proto::Aggregation::UNKNOWN)
    return ValidationResult::kFeatureAggregationNotFound;

  if (GetExpectedTensorLength(feature) != feature.tensor_length())
    return ValidationResult::kFeatureTensorLengthInvalid;

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataAndFeatures(
    const proto::SegmentationModelMetadata& model_metadata) {
  auto metadata_result = ValidateMetadata(model_metadata);
  if (metadata_result != ValidationResult::kValidationSuccess)
    return metadata_result;

  for (int i = 0; i < model_metadata.features_size(); ++i) {
    auto feature = model_metadata.features(i);
    auto feature_result = ValidateMetadataFeature(feature);
    if (feature_result != ValidationResult::kValidationSuccess)
      return feature_result;
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateSegmentInfoMetadataAndFeatures(
    const proto::SegmentInfo& segment_info) {
  auto segment_info_result = ValidateSegmentInfo(segment_info);
  if (segment_info_result != ValidationResult::kValidationSuccess)
    return segment_info_result;

  return ValidateMetadataAndFeatures(segment_info.model_metadata());
}

void SetFeatureNameHashesFromName(
    proto::SegmentationModelMetadata* model_metadata) {
  for (int i = 0; i < model_metadata->features_size(); ++i) {
    proto::Feature* feature = model_metadata->mutable_features(i);
    feature->set_name_hash(base::HashMetricName(feature->name()));
  }
}

bool HasExpiredOrUnavailableResult(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_prediction_result())
    return true;

  base::Time last_result_timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          segment_info.prediction_result().timestamp_us()));

  base::TimeDelta result_ttl =
      segment_info.model_metadata().result_time_to_live() *
      GetTimeUnit(segment_info.model_metadata());

  return last_result_timestamp + result_ttl < base::Time::Now();
}

bool HasFreshResults(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_prediction_result())
    return false;

  const proto::SegmentationModelMetadata& metadata =
      segment_info.model_metadata();

  base::Time last_result_timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          segment_info.prediction_result().timestamp_us()));
  base::TimeDelta result_ttl =
      metadata.result_time_to_live() * GetTimeUnit(metadata);

  return base::Time::Now() - last_result_timestamp < result_ttl;
}

base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata) {
  proto::TimeUnit time_unit = model_metadata.time_unit();
  switch (time_unit) {
    case proto::TimeUnit::YEAR:
      return base::TimeDelta::FromDays(365);
    case proto::TimeUnit::MONTH:
      return base::TimeDelta::FromDays(30);
    case proto::TimeUnit::WEEK:
      return base::TimeDelta::FromDays(7);
    case proto::TimeUnit::DAY:
      return base::TimeDelta::FromDays(1);
    case proto::TimeUnit::HOUR:
      return base::TimeDelta::FromHours(1);
    case proto::TimeUnit::MINUTE:
      return base::TimeDelta::FromMinutes(1);
    case proto::TimeUnit::SECOND:
      return base::TimeDelta::FromSeconds(1);
    case proto::TimeUnit::UNKNOWN_TIME_UNIT:
      FALLTHROUGH;
    default:
      NOTREACHED();
      return base::TimeDelta();
  }
}

SignalKey::Kind SignalTypeToSignalKind(proto::SignalType signal_type) {
  switch (signal_type) {
    case proto::SignalType::USER_ACTION:
      return SignalKey::Kind::USER_ACTION;
    case proto::SignalType::HISTOGRAM_ENUM:
      return SignalKey::Kind::HISTOGRAM_ENUM;
    case proto::SignalType::HISTOGRAM_VALUE:
      return SignalKey::Kind::HISTOGRAM_VALUE;
    case proto::SignalType::UNKNOWN_SIGNAL_TYPE:
      return SignalKey::Kind::UNKNOWN;
  }
}

}  // namespace metadata_utils
}  // namespace segmentation_platform
