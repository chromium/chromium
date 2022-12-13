// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_UTILS_H_

#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
using proto::SegmentId;

namespace metadata_utils {

// Keep up to date with SegmentationPlatformValidationResult in
// //tools/metrics/histograms/enums.xml.
enum class ValidationResult {
  kValidationSuccess = 0,
  kSegmentIDNotFound = 1,
  kMetadataNotFound = 2,
  kTimeUnitInvald = 3,
  kSignalTypeInvalid = 4,
  kFeatureNameNotFound = 5,
  kFeatureNameHashNotFound = 6,
  kFeatureAggregationNotFound = 7,
  kFeatureTensorLengthInvalid = 8,
  kFeatureNameHashDoesNotMatchName = 9,
  kVersionNotSupported = 10,
  kFeatureListInvalid = 11,
  kCustomInputInvalid = 12,
  kFeatureSqlQueryEmpty = 13,
  kFeatureBindValuesInvalid = 14,
  kIndexedTensorsInvalid = 15,
  kMaxValue = kIndexedTensorsInvalid,
};

// Whether the given SegmentInfo and its metadata is valid to be used for the
// current segmentation platform.
ValidationResult ValidateSegmentInfo(const proto::SegmentInfo& segment_info);

// Whether the given metadata is valid to be used for the current segmentation
// platform.
ValidationResult ValidateMetadata(
    const proto::SegmentationModelMetadata& model_metadata);

// Whether the given UMA feature metadata is valid to be used for the current
// segmentation platform.
ValidationResult ValidateMetadataUmaFeature(const proto::UMAFeature& feature);

// Whether the given SQL feature metadata is valid to be used for the current
// segmentation platform.
ValidationResult ValidateMetadataSqlFeature(const proto::SqlFeature& feature);

// Whether the given custom input metadata is valid to be used for the current
// segmentation platform.
ValidationResult ValidateMetadataCustomInput(
    const proto::CustomInput& custom_input);

// Whether the given metadata and feature metadata is valid to be used for the
// current segmentation platform.
ValidationResult ValidateMetadataAndFeatures(
    const proto::SegmentationModelMetadata& model_metadata);

// Whether the given indexed tensor is valid to be used for the current
// segmentation platform.
ValidationResult ValidateIndexedTensors(
    const processing::IndexedTensors& tensor,
    size_t expected_size);

// Whether the given SegmentInfo, metadata and feature metadata is valid to be
// used for the current segmentation platform.
ValidationResult ValidateSegmentInfoMetadataAndFeatures(
    const proto::SegmentInfo& segment_info);

// For all features in the given metadata, updates the feature name hash based
// on the feature name. Note: This mutates the metadata that is passed in.
void SetFeatureNameHashesFromName(
    proto::SegmentationModelMetadata* model_metadata);

// Whether a segment has expired results or no result. Called to determine
// whether the model should be rerun.
bool HasExpiredOrUnavailableResult(const proto::SegmentInfo& segment_info,
                                   const base::Time& now);

// Whether the results were computed too recently for a given segment. If
// true, the model execution should be skipped for the time being.
bool HasFreshResults(const proto::SegmentInfo& segment_info,
                     const base::Time& now);

// Helper method to read the time unit from the proto.
base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata);

// Conversion methods between SignalKey::Kind and proto::SignalType.
SignalKey::Kind SignalTypeToSignalKind(proto::SignalType signal_type);

// Helper method to convert continuous to discrete score.
float ConvertToDiscreteScore(const std::string& mapping_key,
                             float input_score,
                             const proto::SegmentationModelMetadata& metadata);

std::string SegmetationModelMetadataToString(
    const proto::SegmentationModelMetadata& model_metadata);

// Helper method to get all UMAFeatures from a segmentation model's metadata.
// When |include_outputs| is true, the UMA features for training outputs will be
// included. Otherwise only input UMA features are included.
std::vector<proto::UMAFeature> GetAllUmaFeatures(
    const proto::SegmentationModelMetadata& model_metadata,
    bool include_outputs);

// Creates prediction result for a given segment.
proto::PredictionResult CreatePredictionResult(
    const std::vector<float>& model_scores,
    const proto::OutputConfig& output_config,
    base::Time timestamp);

}  // namespace metadata_utils
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_UTILS_H_
