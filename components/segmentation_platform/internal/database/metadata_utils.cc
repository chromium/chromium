// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/internal/segmentation_platform_features.h"
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
  if (!segment_info.has_segment_id())
    return ValidationResult::SEGMENT_ID_NOT_FOUND;

  if (!segment_info.has_model_metadata())
    return ValidationResult::METADATA_NOT_FOUND;

  return ValidateMetadata(segment_info.model_metadata());
}

ValidationResult ValidateMetadata(
    const proto::SegmentationModelMetadata& model_metadata) {
  if (model_metadata.time_unit() == proto::TimeUnit::UNKNOWN_TIME_UNIT)
    return ValidationResult::TIME_UNIT_INVALID;

  return ValidationResult::VALIDATION_SUCCESS;
}

ValidationResult ValidateMetadataFeature(const proto::Feature& feature) {
  auto signal_type = GetSignalTypeForFeature(feature);
  if (signal_type == proto::SignalType::UNKNOWN_SIGNAL_TYPE) {
    return ValidationResult::SIGNAL_TYPE_INVALID;
  }

  if ((signal_type == proto::SignalType::HISTOGRAM_ENUM ||
       signal_type == proto::SignalType::HISTOGRAM_VALUE) &&
      !feature.has_name()) {
    return ValidationResult::FEATURE_NAME_NOT_FOUND;
  }

  if (!GetNameHashForFeature(feature).has_value())
    return ValidationResult::FEATURE_NAME_HASH_NOT_FOUND;

  if (!feature.has_aggregation())
    return ValidationResult::FEATURE_AGGREGATION_NOT_FOUND;

  if (!feature.has_bucket_count())
    return ValidationResult::FEATURE_BUCKET_COUNT_NOT_FOUND;

  if (!feature.has_tensor_length())
    return ValidationResult::FEATURE_TENSOR_LENGTH_NOT_FOUND;

  if (GetExpectedTensorLength(feature) != feature.tensor_length())
    return ValidationResult::FEATURE_TENSOR_LENGTH_INVALID;

  return ValidationResult::VALIDATION_SUCCESS;
}

bool HasExpiredOrUnavailableResult(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_prediction_result())
    return true;

  base::Time last_result_timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          segment_info.prediction_result().timestamp_us()));

  DCHECK(segment_info.has_model_metadata());
  base::TimeDelta result_ttl =
      segment_info.model_metadata().result_time_to_live() *
      GetTimeUnit(segment_info.model_metadata());

  return last_result_timestamp + result_ttl < base::Time::Now();
}

bool HasFreshResults(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_prediction_result())
    return false;

  base::Time last_result_timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          segment_info.prediction_result().timestamp_us()));

  return base::Time::Now() - last_result_timestamp <
         features::GetMinDelayForModelRerun();
}

base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata) {
  DCHECK(model_metadata.has_time_unit());
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

absl::optional<uint64_t> GetNameHashForFeature(const proto::Feature& feature) {
  if (!feature.has_name_hash())
    return absl::nullopt;

  return feature.name_hash();
}

proto::SignalType GetSignalTypeForFeature(const proto::Feature& feature) {
  if (!feature.has_type())
    return proto::SignalType::UNKNOWN_SIGNAL_TYPE;

  return feature.type();
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
