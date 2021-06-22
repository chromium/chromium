// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "base/notreached.h"

namespace segmentation_platform {
namespace metadata_utils {
namespace {
// Used to determine if the model was executed too recently to run again.
// TODO(shaktisahu): Make this finch configurable.
constexpr base::TimeDelta kFreshResultsDurationThreshold =
    base::TimeDelta::FromHours(24);

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
         kFreshResultsDurationThreshold;
}

base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata) {
  DCHECK(model_metadata.has_time_unit());
  proto::TimeUnit time_unit = model_metadata.time_unit();
  switch (time_unit) {
    case proto::TimeUnit::YEAR:
      return base::TimeDelta::FromDays(1) * 365;
    case proto::TimeUnit::MONTH:
      return base::TimeDelta::FromDays(1) * 30;
    case proto::TimeUnit::WEEK:
      return base::TimeDelta::FromDays(1) * 7;
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

}  // namespace metadata_utils
}  // namespace segmentation_platform
