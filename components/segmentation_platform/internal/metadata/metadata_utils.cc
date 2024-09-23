// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"

#include <inttypes.h>

#include "base/metrics/metrics_hashes.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform::metadata_utils {

namespace {
uint64_t GetExpectedTensorLength(const proto::UMAFeature& feature) {
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
    case proto::Aggregation::LATEST_OR_DEFAULT:
      return 1;
    case proto::Aggregation::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

std::string FeatureToString(const proto::UMAFeature& feature) {
  std::string result;
  if (feature.has_type())
    result = "type:" + proto::SignalType_Name(feature.type()) + ", ";
  if (feature.has_name())
    result.append("name:" + feature.name() + ", ");
  if (feature.has_name_hash()) {
    result.append(
        base::StringPrintf("name_hash:0x%" PRIx64 ", ", feature.name_hash()));
  }
  if (feature.has_bucket_count()) {
    result.append(base::StringPrintf("bucket_count:%" PRIu64 ", ",
                                     feature.bucket_count()));
  }
  if (feature.has_tensor_length()) {
    result.append(base::StringPrintf("tensor_length:%" PRIu64 ", ",
                                     feature.tensor_length()));
  }
  if (feature.has_aggregation()) {
    result.append("aggregation:" +
                  proto::Aggregation_Name(feature.aggregation()));
  }
  if (base::EndsWith(result, ", "))
    result.resize(result.size() - 2);
  return result;
}

ValidationResult ValidateBinaryClassifier(
    const proto::Predictor::BinaryClassifier& classifier) {
  if (classifier.positive_label().empty() ||
      classifier.negative_label().empty()) {
    return ValidationResult::kBinaryClassifierEmptyLabels;
  }
  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateBinnedClassifier(
    const proto::Predictor::BinnedClassifier& classifier) {
  float prev = std::numeric_limits<float>::lowest();
  for (const auto& bin : classifier.bins()) {
    if (bin.label().empty()) {
      return ValidationResult::kBinnedClassifierEmptyLabels;
    }
    if (bin.min_range() < prev) {
      return ValidationResult::kBinnedClassifierBinsUnsorted;
    }
    prev = bin.min_range();
  }
  return ValidationResult::kValidationSuccess;
}

}  // namespace

ValidationResult ValidateSegmentInfo(const proto::SegmentInfo& segment_info) {
  if (segment_info.segment_id() == proto::OPTIMIZATION_TARGET_UNKNOWN) {
    return ValidationResult::kSegmentIDNotFound;
  }

  if (!segment_info.has_model_metadata())
    return ValidationResult::kMetadataNotFound;

  return ValidateMetadata(segment_info.model_metadata());
}

ValidationResult ValidateMetadata(
    const proto::SegmentationModelMetadata& model_metadata) {
  if (proto::CurrentVersion::METADATA_VERSION <
      model_metadata.version_info().metadata_min_version()) {
    return ValidationResult::kVersionNotSupported;
  }

  if (model_metadata.time_unit() == proto::TimeUnit::UNKNOWN_TIME_UNIT)
    return ValidationResult::kTimeUnitInvald;

  if (model_metadata.features_size() != 0 &&
      model_metadata.input_features_size() != 0) {
    return ValidationResult::kFeatureListInvalid;
  }

  if (model_metadata.has_output_config() &&
      model_metadata.discrete_mappings_size() > 0) {
    return ValidationResult::kDiscreteMappingAndOutputConfigFound;
  }

  if (model_metadata.has_output_config()) {
    auto output_config_result =
        ValidateOutputConfig(model_metadata.output_config());
    if (output_config_result != ValidationResult::kValidationSuccess) {
      return output_config_result;
    }
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataUmaFeature(const proto::UMAFeature& feature) {
  if (feature.type() == proto::SignalType::UNKNOWN_SIGNAL_TYPE ||
      feature.type() == proto::SignalType::UKM_EVENT)
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

ValidationResult ValidateMetadataSqlFeature(const proto::SqlFeature& feature) {
  if (feature.sql().empty())
    return ValidationResult::kFeatureSqlQueryEmpty;

  int total_tensor_length = 0;
  for (int i = 0; i < feature.bind_values_size(); ++i) {
    const auto& bind_value = feature.bind_values(i);
    if (!bind_value.has_value() ||
        bind_value.param_type() == proto::SqlFeature::BindValue::UNKNOWN ||
        ValidateMetadataCustomInput(bind_value.value()) !=
            ValidationResult::kValidationSuccess) {
      return ValidationResult::kFeatureBindValuesInvalid;
    }
    total_tensor_length += bind_value.value().tensor_length();
  }

  if (total_tensor_length != base::ranges::count(feature.sql(), '?')) {
    return ValidationResult::kFeatureBindValuesInvalid;
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataCustomInput(
    const proto::CustomInput& custom_input) {
  if (custom_input.fill_policy() == proto::CustomInput::UNKNOWN_FILL_POLICY) {
    // If the current fill policy is not supported or not filled, we must use
    // the given default value list, therefore the default value list must
    // provide enough input values as specified by tensor length.
    if (custom_input.tensor_length() > custom_input.default_value_size()) {
      return ValidationResult::kCustomInputInvalid;
    }
  } else if (custom_input.default_value_size() != 0) {
    // The default value should be longer than the tensor length.
    if (custom_input.tensor_length() > custom_input.default_value_size()) {
      return ValidationResult::kCustomInputInvalid;
    }
  }
  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataAndFeatures(
    const proto::SegmentationModelMetadata& model_metadata) {
  auto metadata_result = ValidateMetadata(model_metadata);
  if (metadata_result != ValidationResult::kValidationSuccess)
    return metadata_result;

  if (model_metadata.has_output_config()) {
    auto output_config_result =
        ValidateOutputConfig(model_metadata.output_config());
    if (output_config_result != ValidationResult::kValidationSuccess) {
      return output_config_result;
    }
  }

  for (int i = 0; i < model_metadata.features_size(); ++i) {
    const auto& feature = model_metadata.features(i);
    auto feature_result = ValidateMetadataUmaFeature(feature);
    if (feature_result != ValidationResult::kValidationSuccess)
      return feature_result;
  }

  for (int i = 0; i < model_metadata.input_features_size(); ++i) {
    const auto& feature = model_metadata.input_features(i);
    if (feature.has_uma_feature()) {
      auto feature_result = ValidateMetadataUmaFeature(feature.uma_feature());
      if (feature_result != ValidationResult::kValidationSuccess)
        return feature_result;
    } else if (feature.has_custom_input()) {
      auto feature_result = ValidateMetadataCustomInput(feature.custom_input());
      if (feature_result != ValidationResult::kValidationSuccess)
        return feature_result;
    } else if (feature.has_sql_feature()) {
      // TODO(haileywang): Fix sql validation with other requirements.
      if (feature.sql_feature().sql().empty())
        return ValidationResult::kFeatureListInvalid;
    } else {
      return ValidationResult::kFeatureListInvalid;
    }
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateIndexedTensors(
    const processing::IndexedTensors& tensor,
    size_t expected_size) {
  if (tensor.size() != expected_size)
    return ValidationResult::kIndexedTensorsInvalid;
  for (size_t i = 0; i < tensor.size(); ++i) {
    if (tensor.count(i) != 1)
      return ValidationResult::kIndexedTensorsInvalid;
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

ValidationResult ValidateOutputConfig(
    const proto::OutputConfig& output_config) {
  if (!output_config.has_predictor()) {
    return ValidationResult::kPredictorTypeMissing;
  }
  const auto& predictor = output_config.predictor();
  if (predictor.has_generic_predictor()) {
    if (predictor.generic_predictor().output_labels_size() == 0) {
      return ValidationResult::kGenericPredictorMissingLabels;
    }
  }
  if (predictor.has_binary_classifier()) {
    ValidationResult result =
        ValidateBinaryClassifier(predictor.binary_classifier());
    if (result != ValidationResult::kValidationSuccess) {
      return result;
    }
  }
  if (predictor.has_binned_classifier()) {
    ValidationResult result =
        ValidateBinnedClassifier(predictor.binned_classifier());
    if (result != ValidationResult::kValidationSuccess) {
      return result;
    }
  }
  if (predictor.has_multi_class_classifier()) {
    ValidationResult result =
        ValidateMultiClassClassifier(predictor.multi_class_classifier());
    if (result != ValidationResult::kValidationSuccess) {
      return result;
    }
  }

  if (output_config.has_predicted_result_ttl()) {
    const auto& result_ttl = output_config.predicted_result_ttl();
    if (!result_ttl.has_default_ttl()) {
      return ValidationResult::kDefaultTtlIsMissing;
    }
    if (result_ttl.time_unit() == proto::TimeUnit::UNKNOWN_TIME_UNIT) {
      return ValidationResult::kPredictionTtlTimeUnitInvalid;
    }
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMultiClassClassifier(
    const proto::Predictor_MultiClassClassifier& multi_class_classifier) {
  if (multi_class_classifier.class_labels_size() == 0) {
    return ValidationResult::kMultiClassClassifierHasNoLabels;
  }

  if (multi_class_classifier.has_threshold() &&
      multi_class_classifier.class_thresholds_size() > 0) {
    return ValidationResult::kMultiClassClassifierUsesBothThresholdTypes;
  }

  if (multi_class_classifier.class_thresholds_size() > 0 &&
      multi_class_classifier.class_thresholds_size() !=
          multi_class_classifier.class_labels_size()) {
    return ValidationResult::
        kMultiClassClassifierClassAndThresholdCountMismatch;
  }

  return ValidationResult::kValidationSuccess;
}

void SetFeatureNameHashesFromName(
    proto::SegmentationModelMetadata* model_metadata) {
  for (int i = 0; i < model_metadata->features_size(); ++i) {
    proto::UMAFeature* feature = model_metadata->mutable_features(i);
    feature->set_name_hash(base::HashMetricName(feature->name()));
  }
}

bool HasExpiredOrUnavailableResult(const proto::SegmentInfo& segment_info,
                                   const base::Time& now) {
  if (!segment_info.has_prediction_result())
    return true;
  if (segment_info.prediction_result().result_size() == 0) {
    return true;
  }

  base::Time last_result_timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(segment_info.prediction_result().timestamp_us()));

  base::TimeDelta result_ttl =
      segment_info.model_metadata().result_time_to_live() *
      GetTimeUnit(segment_info.model_metadata());

  return last_result_timestamp + result_ttl < now;
}

bool HasFreshResults(const proto::SegmentInfo& segment_info,
                     const base::Time& now) {
  if (!segment_info.has_prediction_result())
    return false;

  const proto::SegmentationModelMetadata& metadata =
      segment_info.model_metadata();

  base::Time last_result_timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(segment_info.prediction_result().timestamp_us()));
  base::TimeDelta result_ttl =
      metadata.result_time_to_live() * GetTimeUnit(metadata);

  return now - last_result_timestamp < result_ttl;
}

base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata) {
  proto::TimeUnit time_unit = model_metadata.time_unit();
  return ConvertToTimeDelta(time_unit);
}

base::TimeDelta ConvertToTimeDelta(proto::TimeUnit time_unit) {
  switch (time_unit) {
    case proto::TimeUnit::YEAR:
      return base::Days(365);
    case proto::TimeUnit::MONTH:
      return base::Days(30);
    case proto::TimeUnit::WEEK:
      return base::Days(7);
    case proto::TimeUnit::DAY:
      return base::Days(1);
    case proto::TimeUnit::HOUR:
      return base::Hours(1);
    case proto::TimeUnit::MINUTE:
      return base::Minutes(1);
    case proto::TimeUnit::SECOND:
      return base::Seconds(1);
    case proto::TimeUnit::UNKNOWN_TIME_UNIT:
      [[fallthrough]];
    default:
      NOTREACHED_IN_MIGRATION();
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
    case proto::SignalType::UKM_EVENT:
    case proto::SignalType::UNKNOWN_SIGNAL_TYPE:
      return SignalKey::Kind::UNKNOWN;
  }
}

proto::SignalType SignalKindToSignalType(SignalKey::Kind kind) {
  switch (kind) {
    case SignalKey::Kind::USER_ACTION:
      return proto::SignalType::USER_ACTION;
    case SignalKey::Kind::HISTOGRAM_ENUM:
      return proto::SignalType::HISTOGRAM_ENUM;
    case SignalKey::Kind::HISTOGRAM_VALUE:
      return proto::SignalType::HISTOGRAM_VALUE;
    case SignalKey::Kind::UNKNOWN:
      return proto::SignalType::UNKNOWN_SIGNAL_TYPE;
  }
}

float ConvertToDiscreteScore(const std::string& mapping_key,
                             float input_score,
                             const proto::SegmentationModelMetadata& metadata) {
  auto iter = metadata.discrete_mappings().find(mapping_key);
  if (iter == metadata.discrete_mappings().end()) {
    iter =
        metadata.discrete_mappings().find(metadata.default_discrete_mapping());
    if (iter == metadata.discrete_mappings().end())
      return input_score;
  }
  CHECK(iter != metadata.discrete_mappings().end(), base::NotFatalUntil::M130);

  const auto& mapping = iter->second;

  // Iterate over the entries and find the largest entry whose min result is
  // equal to or less than the input.
  int discrete_result = 0;
  float largest_score_below_input_score = std::numeric_limits<float>::min();
  for (int i = 0; i < mapping.entries_size(); i++) {
    const auto& entry = mapping.entries(i);
    if (entry.min_result() <= input_score &&
        entry.min_result() > largest_score_below_input_score) {
      largest_score_below_input_score = entry.min_result();
      discrete_result = entry.rank();
    }
  }

  return discrete_result;
}

std::string SegmetationModelMetadataToString(
    const proto::SegmentationModelMetadata& model_metadata) {
  std::string result;
  for (const auto& feature : model_metadata.features()) {
    result.append("feature:{" + FeatureToString(feature) + "}, ");
  }
  if (model_metadata.has_time_unit()) {
    result.append(
        "time_unit:" + proto::TimeUnit_Name(model_metadata.time_unit()) + ", ");
  }
  if (model_metadata.has_bucket_duration()) {
    result.append(base::StringPrintf("bucket_duration:%" PRIu64 ", ",
                                     model_metadata.bucket_duration()));
  }
  if (model_metadata.has_signal_storage_length()) {
    result.append(base::StringPrintf("signal_storage_length:%" PRId64 ", ",
                                     model_metadata.signal_storage_length()));
  }
  if (model_metadata.has_min_signal_collection_length()) {
    result.append(
        base::StringPrintf("min_signal_collection_length:%" PRId64 ", ",
                           model_metadata.min_signal_collection_length()));
  }
  if (model_metadata.has_result_time_to_live()) {
    result.append(base::StringPrintf("result_time_to_live:%" PRId64 ", ",
                                     model_metadata.result_time_to_live()));
  }
  if (model_metadata.has_upload_tensors()) {
    result.append(
        base::StringPrintf("upload_tensors: %s",
                           model_metadata.upload_tensors() ? "true" : "false"));
  }

  if (base::EndsWith(result, ", "))
    result.resize(result.size() - 2);
  return result;
}

std::vector<proto::UMAFeature> GetAllUmaFeatures(
    const proto::SegmentationModelMetadata& model_metadata,
    bool include_outputs) {
  std::vector<proto::UMAFeature> features;
  VisitAllUmaFeatures(model_metadata, include_outputs,
                      base::BindRepeating(
                          [](std::vector<proto::UMAFeature>* features,
                             const proto::UMAFeature& feature) {
                            features->push_back(feature);
                          },
                          base::Unretained(&features)));
  return features;
}

using VisitUmaFeature =
    base::RepeatingCallback<void(const proto::UMAFeature& feature)>;
void VisitAllUmaFeatures(const proto::SegmentationModelMetadata& model_metadata,
                         bool include_outputs,
                         VisitUmaFeature visit) {
  std::vector<proto::UMAFeature> features;
  for (int i = 0; i < model_metadata.features_size(); ++i) {
    visit.Run(model_metadata.features(i));
  }

  // Add training/inference inputs.
  for (int i = 0; i < model_metadata.input_features_size(); ++i) {
    const auto& feature = model_metadata.input_features(i);
    if (feature.has_uma_feature())
      visit.Run(feature.uma_feature());
  }

  // Add training/inference outputs.
  if (include_outputs) {
    for (const auto& output : model_metadata.training_outputs().outputs()) {
      DCHECK(output.has_uma_output()) << "Currently only support UMA output.";
      if (output.has_uma_output())
        visit.Run(output.uma_output().uma_feature());
    }
  }

  // Add uma trigger features.
  if (model_metadata.training_outputs().has_trigger_config()) {
    const auto& training_config =
        model_metadata.training_outputs().trigger_config();
    for (int i = 0; i < training_config.observation_trigger_size(); i++) {
      const auto& trigger = training_config.observation_trigger(i);
      if (trigger.has_uma_trigger() &&
          trigger.uma_trigger().has_uma_feature()) {
        visit.Run(trigger.uma_trigger().uma_feature());
      }
    }
  }
}

proto::PredictionResult CreatePredictionResult(
    const std::vector<float>& model_scores,
    const proto::OutputConfig& output_config,
    base::Time timestamp,
    int64_t model_version) {
  proto::PredictionResult result;
  result.mutable_result()->Add(model_scores.begin(), model_scores.end());
  if (output_config.has_predictor()) {
    result.mutable_output_config()->CopyFrom(output_config);
  }
  result.set_timestamp_us(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  result.set_model_version(model_version);
  return result;
}

proto::ClientResult CreateClientResultFromPredResult(
    proto::PredictionResult pred_result,
    base::Time timestamp) {
  proto::ClientResult client_result;
  client_result.mutable_client_result()->CopyFrom(pred_result);
  client_result.set_timestamp_us(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return client_result;
}

bool ConfigUsesLegacyOutput(const Config* config) {
  return (config->segments.size() >= 1 &&
          SegmentUsesLegacyOutput(config->segments.begin()->first));
}

bool SegmentUsesLegacyOutput(proto::SegmentId segment_id) {
  // List of segments ids that doesn't support multi output and uses
  // legacy output. Please delete `SegmentId` from this list if segment is
  // migrated to support multi output.
  base::flat_set<SegmentId> segment_ids_use_legacy{
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE};

  return segment_ids_use_legacy.contains(segment_id);
}

}  // namespace segmentation_platform::metadata_utils
