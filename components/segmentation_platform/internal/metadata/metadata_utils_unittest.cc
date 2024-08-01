// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"

#include "base/metrics/metrics_hashes.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

void AddDiscreteMapping(proto::SegmentationModelMetadata* metadata,
                        float mappings[][2],
                        int num_pairs,
                        const std::string& discrete_mapping_key) {
  auto* discrete_mappings_map = metadata->mutable_discrete_mappings();
  auto& discrete_mappings = (*discrete_mappings_map)[discrete_mapping_key];
  for (int i = 0; i < num_pairs; i++) {
    auto* pair = mappings[i];
    auto* entry = discrete_mappings.add_entries();
    entry->set_min_result(pair[0]);
    entry->set_rank(pair[1]);
  }
}

}  // namespace

class MetadataUtilsTest : public testing::Test {
 public:
  ~MetadataUtilsTest() override = default;
};

TEST_F(MetadataUtilsTest, SegmentInfoValidation) {
  proto::SegmentInfo segment_info;
  EXPECT_EQ(metadata_utils::ValidationResult::kSegmentIDNotFound,
            metadata_utils::ValidateSegmentInfo(segment_info));

  segment_info.set_segment_id(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  EXPECT_EQ(metadata_utils::ValidationResult::kMetadataNotFound,
            metadata_utils::ValidateSegmentInfo(segment_info));

  // The rest of this test verifies that at least some metadata is verified.
  segment_info.mutable_model_metadata()->set_time_unit(
      proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateSegmentInfo(segment_info));

  segment_info.mutable_model_metadata()->set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateSegmentInfo(segment_info));

  EXPECT_FALSE(segment_info.has_model_source());
  segment_info.set_model_source(proto::ModelSource::DEFAULT_MODEL_SOURCE);
  EXPECT_EQ(proto::ModelSource::DEFAULT_MODEL_SOURCE,
            segment_info.model_source());
}

TEST_F(MetadataUtilsTest, ValidatingPredictionResultOptionalVsRepeated) {
  proto::SegmentInfo segment_info;
  // PredictionResult with repeated float result.
  proto::PredictionResult result;

  // Serialised string for optional float result = 0.8.
  proto::LegacyPredictionResultForTesting legacy_result;
  legacy_result.set_result(0.8);
  std::string serialised_result_without_repeated =
      legacy_result.SerializeAsString();

  // Serialised string for repeated float result = {0.8}
  segment_info.mutable_prediction_result()->add_result(0.8);
  std::string serialised_result_with_repeated =
      segment_info.prediction_result().SerializeAsString();

  EXPECT_EQ(serialised_result_without_repeated,
            serialised_result_with_repeated);

  // Deserialising the serialised string back.
  result.ParseFromString(serialised_result_without_repeated);
  EXPECT_THAT(result.result(), testing::ElementsAre(0.8f));
}

TEST_F(MetadataUtilsTest, DefaultMetadataIsInvalid) {
  proto::SegmentationModelMetadata empty;

  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateMetadata(empty));
}

TEST_F(MetadataUtilsTest, MetadataValidation) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateMetadata(metadata));

  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadata(metadata));

  proto::VersionInfo* version_info = metadata.mutable_version_info();
  version_info->set_metadata_min_version(
      proto::CurrentVersion::METADATA_VERSION + 1);
  EXPECT_EQ(metadata_utils::ValidationResult::kVersionNotSupported,
            metadata_utils::ValidateMetadata(metadata));
}

TEST_F(MetadataUtilsTest, MetadataUmaFeatureValidation) {
  proto::UMAFeature feature;
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_type(proto::SignalType::UNKNOWN_SIGNAL_TYPE);
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_type(proto::SignalType::UKM_EVENT);
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  // name not required for USER_ACTION.
  feature.set_type(proto::SignalType::USER_ACTION);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameHashNotFound,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_ENUM);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameNotFound,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_VALUE);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameNotFound,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_name("test name");
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameHashNotFound,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_name_hash(base::HashMetricName("not the correct name"));
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameHashDoesNotMatchName,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_name_hash(base::HashMetricName("test name"));
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureAggregationNotFound,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_aggregation(proto::Aggregation::COUNT);
  // No bucket_count or tensor_length is valid.
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  feature.set_bucket_count(456);
  // Aggregation=COUNT requires tensor length = 1.
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
            metadata_utils::ValidateMetadataUmaFeature(feature));

  std::vector<proto::Aggregation> tensor_length_1 = {
      proto::Aggregation::COUNT,
      proto::Aggregation::COUNT_BOOLEAN,
      proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT,
      proto::Aggregation::SUM,
      proto::Aggregation::SUM_BOOLEAN,
      proto::Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT,
  };
  std::vector<proto::Aggregation> tensor_length_bucket_count = {
      proto::Aggregation::BUCKETED_COUNT,
      proto::Aggregation::BUCKETED_COUNT_BOOLEAN,
      proto::Aggregation::BUCKETED_CUMULATIVE_COUNT,
      proto::Aggregation::BUCKETED_SUM,
      proto::Aggregation::BUCKETED_SUM_BOOLEAN,
      proto::Aggregation::BUCKETED_CUMULATIVE_SUM,
  };

  for (auto aggregation : tensor_length_1) {
    feature.set_aggregation(aggregation);

    // If bucket count is 0, do not use for output, i.e. tensor_length should be
    // 0.
    feature.set_bucket_count(0);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataUmaFeature(feature));
    feature.set_tensor_length(0);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataUmaFeature(feature));

    // Tensor length should otherwise always be 1 for this aggregation type.
    feature.set_bucket_count(456);
    feature.set_tensor_length(10);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataUmaFeature(feature));

    feature.set_bucket_count(456);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataUmaFeature(feature));
  }

  for (auto aggregation : tensor_length_bucket_count) {
    feature.set_aggregation(aggregation);

    // If bucket count is 0, do not use for output, i.e. tensor_length should be
    // 0.
    feature.set_bucket_count(0);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataUmaFeature(feature));
    feature.set_tensor_length(0);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataUmaFeature(feature));

    // Tensor length should otherwise always be equal to bucket_count for this
    // aggregation type.
    feature.set_bucket_count(456);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataUmaFeature(feature));

    feature.set_bucket_count(456);
    feature.set_tensor_length(456);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataUmaFeature(feature));
  }
}

TEST_F(MetadataUtilsTest, MetadataSqlFeatureValidation) {
  // Sql feature with no sql query string is invalid.
  proto::SqlFeature sql_feature;
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureSqlQueryEmpty,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));

  sql_feature.set_sql("sql query");
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));

  // Sql feature with a bind value with no value is invalid.
  auto* bind_value = sql_feature.add_bind_values();
  bind_value->set_param_type(proto::SqlFeature::BindValue::BOOL);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureBindValuesInvalid,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));

  bind_value->mutable_value();
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));

  // Sql feature with a bind value of type unknown is invalid.
  bind_value->set_param_type(proto::SqlFeature::BindValue::UNKNOWN);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureBindValuesInvalid,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));
}

TEST_F(MetadataUtilsTest, MetadataSqlFeatureTensorLengthValidation) {
  // The number of "?" in the query string should be equal to the total of
  // bind_value's tensor_length.
  proto::SqlFeature sql_feature;
  sql_feature.set_sql("one bind_value ? ? ?");

  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureBindValuesInvalid,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));

  // Add a bind_value with tensor length of 1.
  auto* bind_value = sql_feature.add_bind_values();
  bind_value->set_param_type(proto::SqlFeature::BindValue::BOOL);
  auto* custom_input = bind_value->mutable_value();
  custom_input->set_tensor_length(1);
  custom_input->add_default_value(0);

  // Add a bind_value with tensor length of 2.
  auto* bind_value2 = sql_feature.add_bind_values();
  bind_value2->set_param_type(proto::SqlFeature::BindValue::BOOL);
  auto* custom_input2 = bind_value2->mutable_value();
  custom_input2->set_tensor_length(2);
  custom_input2->add_default_value(0);
  custom_input2->add_default_value(0);

  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataSqlFeature(sql_feature));
}

TEST_F(MetadataUtilsTest, MetadataCustomInputValidation) {
  // Empty custom input has tensor length of 0 and result in a valid input
  // tensor of length 0.
  proto::CustomInput custom_input;
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataCustomInput(custom_input));

  // When fill policy is unknown, the custom input is invalid if the default
  // value list size is smaller than the input tensor length.
  custom_input.set_tensor_length(1);
  EXPECT_EQ(metadata_utils::ValidationResult::kCustomInputInvalid,
            metadata_utils::ValidateMetadataCustomInput(custom_input));

  custom_input.add_default_value(0);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataCustomInput(custom_input));

  // When default value is filled, tensor length must be smaller or equal than
  // the default value list size.
  custom_input.set_fill_policy(proto::CustomInput::FILL_PREDICTION_TIME);
  custom_input.set_tensor_length(2);
  EXPECT_EQ(metadata_utils::ValidationResult::kCustomInputInvalid,
            metadata_utils::ValidateMetadataCustomInput(custom_input));

  custom_input.set_tensor_length(1);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataCustomInput(custom_input));
}

TEST_F(MetadataUtilsTest, ValidateMetadataAndFeatures) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding a single features adds new requirements.
  auto* feature1 = metadata.add_features();
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out an example feature and verify validation starts working
  // again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(base::HashMetricName("some user action"));
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding another feature adds new requirements again.
  auto* feature2 = metadata.add_features();
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out the second feature and verify validation starts working
  // again.
  feature2->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature2->set_name("some histogram");
  feature2->set_name_hash(base::HashMetricName("some histogram"));
  feature2->set_aggregation(proto::Aggregation::BUCKETED_COUNT);
  feature2->set_bucket_count(2);
  feature2->set_tensor_length(2);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));
}

TEST_F(MetadataUtilsTest, ValidateMetadataAndInputFeatures) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding a single input feature adds new requirements.
  auto* input1 = metadata.add_input_features();
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureListInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  proto::UMAFeature* feature1 = input1->mutable_uma_feature();
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out an example input feature and verify validation starts
  // working again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(base::HashMetricName("some user action"));
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding another feature adds new requirements again.
  auto* input2 = metadata.add_input_features();
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureListInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  proto::UMAFeature* feature2 = input2->mutable_uma_feature();
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out the second feature and verify validation starts working
  // again.
  feature2->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature2->set_name("some histogram");
  feature2->set_name_hash(base::HashMetricName("some histogram"));
  feature2->set_aggregation(proto::Aggregation::BUCKETED_COUNT);
  feature2->set_bucket_count(2);
  feature2->set_tensor_length(2);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify that setting both features and input_features list is invalid.
  auto* feature3 = metadata.add_features();
  feature3->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature3->set_name("some other histogram");
  feature3->set_name_hash(base::HashMetricName("some other histogram"));
  feature3->set_aggregation(proto::Aggregation::BUCKETED_COUNT);
  feature3->set_bucket_count(2);
  feature3->set_tensor_length(2);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureListInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));
}

TEST_F(MetadataUtilsTest, MetadataIndexedTensorsValidation) {
  // Empty indexed tensors are valid.
  processing::QueryProcessor::IndexedTensors tensor;
  EXPECT_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateIndexedTensors(tensor, /* expected_size */ 0));

  // Not continuously indexed tensors are invalid.
  const std::vector<processing::ProcessedValue> value;
  tensor[0] = value;
  tensor[1] = value;
  tensor[3] = value;
  EXPECT_EQ(metadata_utils::ValidationResult::kIndexedTensorsInvalid,
            metadata_utils::ValidateIndexedTensors(
                tensor, /*expected_size*/ tensor.size()));

  tensor[2] = value;
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateIndexedTensors(
                tensor, /*expected_size*/ tensor.size()));

  // The tensor size should match the expected tensor size.
  EXPECT_EQ(metadata_utils::ValidationResult::kIndexedTensorsInvalid,
            metadata_utils::ValidateIndexedTensors(
                tensor, /*expected_size*/ tensor.size() - 1));
}

TEST_F(MetadataUtilsTest, ValidateSegementInfoMetadataAndFeatures) {
  proto::SegmentInfo segment_info;
  EXPECT_EQ(
      metadata_utils::ValidationResult::kSegmentIDNotFound,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  segment_info.set_segment_id(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  EXPECT_EQ(
      metadata_utils::ValidationResult::kMetadataNotFound,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  EXPECT_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  // Verify adding a single features adds new requirements.
  auto* feature1 = metadata->add_features();
  EXPECT_EQ(
      metadata_utils::ValidationResult::kSignalTypeInvalid,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  // Fully flesh out an example feature and verify validation starts working
  // again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(base::HashMetricName("some user action"));
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest, ValidateMultiClassClassifierWithNoClasses) {
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  metadata->mutable_output_config()
      ->mutable_predictor()
      ->mutable_multi_class_classifier();

  EXPECT_EQ(
      metadata_utils::ValidationResult::kMultiClassClassifierHasNoLabels,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest, ValidateMultiClassClassifierWithBothThresholdTypes) {
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  auto* multi_class_classifier = metadata->mutable_output_config()
                                     ->mutable_predictor()
                                     ->mutable_multi_class_classifier();
  multi_class_classifier->add_class_labels("Foo");
  multi_class_classifier->add_class_labels("Bar");

  // Either 'threshold' or 'class_thresholds' should be set, but not both.
  multi_class_classifier->set_threshold(0.5f);

  multi_class_classifier->add_class_thresholds(0.1f);
  multi_class_classifier->add_class_thresholds(0.2f);

  EXPECT_EQ(
      metadata_utils::ValidationResult::
          kMultiClassClassifierUsesBothThresholdTypes,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest,
       ValidateMultiClassClassifierWithClassThresholdCountMismatch) {
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  auto* multi_class_classifier = metadata->mutable_output_config()
                                     ->mutable_predictor()
                                     ->mutable_multi_class_classifier();
  multi_class_classifier->add_class_labels("Foo");
  multi_class_classifier->add_class_labels("Bar");
  multi_class_classifier->add_class_labels("Baz");

  // There are 3 'class_labels' but only 2 'class_thresholds', both should have
  // the same count.
  multi_class_classifier->add_class_thresholds(0.1f);
  multi_class_classifier->add_class_thresholds(0.2f);

  EXPECT_EQ(
      metadata_utils::ValidationResult::
          kMultiClassClassifierClassAndThresholdCountMismatch,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest, ValidateMultiClassClassifierSuccessfully) {
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  auto* multi_class_classifier = metadata->mutable_output_config()
                                     ->mutable_predictor()
                                     ->mutable_multi_class_classifier();
  multi_class_classifier->add_class_labels("Foo");
  multi_class_classifier->add_class_labels("Bar");

  multi_class_classifier->add_class_thresholds(0.1f);
  multi_class_classifier->add_class_thresholds(0.2f);

  EXPECT_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest, SetFeatureNameHashesFromName) {
  // No crashes should happen if there are no features.
  proto::SegmentationModelMetadata empty;
  metadata_utils::SetFeatureNameHashesFromName(&empty);

  // Ensure that the name hash is overwritten.
  proto::SegmentationModelMetadata one_feature_both_set;
  auto* feature1 = one_feature_both_set.add_features();
  feature1->set_name("both set");
  feature1->set_name_hash(base::HashMetricName("both set"));
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_both_set);
  EXPECT_EQ(1, one_feature_both_set.features_size());
  EXPECT_EQ("both set", one_feature_both_set.features(0).name());
  EXPECT_EQ(base::HashMetricName("both set"),
            one_feature_both_set.features(0).name_hash());

  // Ensure that the name hash is overwritten if it is incorrect.
  proto::SegmentationModelMetadata one_feature_both_set_hash_incorrect;
  auto* feature2 = one_feature_both_set_hash_incorrect.add_features();
  feature2->set_name("both set");
  feature2->set_name_hash(base::HashMetricName("INCORRECT NAME HASH"));
  metadata_utils::SetFeatureNameHashesFromName(
      &one_feature_both_set_hash_incorrect);
  EXPECT_EQ(1, one_feature_both_set_hash_incorrect.features_size());
  EXPECT_EQ("both set", one_feature_both_set_hash_incorrect.features(0).name());
  EXPECT_EQ(base::HashMetricName("both set"),
            one_feature_both_set_hash_incorrect.features(0).name_hash());

  // Ensure that the name hash is set from the name.
  proto::SegmentationModelMetadata one_feature_name_set;
  auto* feature3 = one_feature_name_set.add_features();
  feature3->set_name("only name set");
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_name_set);
  EXPECT_EQ(1, one_feature_name_set.features_size());
  EXPECT_EQ("only name set", one_feature_name_set.features(0).name());
  EXPECT_EQ(base::HashMetricName("only name set"),
            one_feature_name_set.features(0).name_hash());

  // Name hash should be overwritten with the hash of the empty string in the
  // case of only the name hash having been set.
  proto::SegmentationModelMetadata one_feature_name_hash_set;
  auto* feature4 = one_feature_name_hash_set.add_features();
  feature4->set_name_hash(base::HashMetricName("only name hash set"));
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_name_hash_set);
  EXPECT_EQ(1, one_feature_name_hash_set.features_size());
  EXPECT_EQ("", one_feature_name_hash_set.features(0).name());
  EXPECT_EQ(base::HashMetricName(""),
            one_feature_name_hash_set.features(0).name_hash());

  // When neither name nor name hash is set, we should still overwrite the name
  // hash with the hash of the empty string.
  proto::SegmentationModelMetadata one_feature_nothing_set;
  // Add feature and set a different field to ensure it is added.
  auto* feature5 = one_feature_nothing_set.add_features();
  feature5->set_type(proto::SignalType::USER_ACTION);
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_nothing_set);
  EXPECT_EQ(1, one_feature_nothing_set.features_size());
  EXPECT_EQ("", one_feature_nothing_set.features(0).name());
  EXPECT_EQ(base::HashMetricName(""),
            one_feature_nothing_set.features(0).name_hash());

  // Ensure that the name hash is set for all features.
  proto::SegmentationModelMetadata multiple_features;
  auto* multifeature1 = multiple_features.add_features();
  multifeature1->set_name("first multi");
  auto* multifeature2 = multiple_features.add_features();
  multifeature2->set_name("second multi");
  metadata_utils::SetFeatureNameHashesFromName(&multiple_features);
  EXPECT_EQ(2, multiple_features.features_size());
  EXPECT_EQ("first multi", multiple_features.features(0).name());
  EXPECT_EQ(base::HashMetricName("first multi"),
            multiple_features.features(0).name_hash());
  EXPECT_EQ("second multi", multiple_features.features(1).name());
  EXPECT_EQ(base::HashMetricName("second multi"),
            multiple_features.features(1).name_hash());
}

TEST_F(MetadataUtilsTest, HasFreshResults) {
  base::Time now = base::Time::Now();
  proto::SegmentInfo segment_info;
  // No result.
  EXPECT_FALSE(metadata_utils::HasFreshResults(segment_info, now));

  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_result_time_to_live(1);
  metadata->set_time_unit(proto::DAY);

  // Stale results.
  auto* prediction_result = segment_info.mutable_prediction_result();
  base::Time result_time = now - base::Days(3);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_FALSE(metadata_utils::HasFreshResults(segment_info, now));

  // Fresh results.
  result_time = now - base::Hours(2);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(metadata_utils::HasFreshResults(segment_info, now));
}

TEST_F(MetadataUtilsTest, HasExpiredOrUnavailableResult) {
  proto::SegmentInfo segment_info;
  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_result_time_to_live(7);
  metadata->set_time_unit(proto::TimeUnit::DAY);
  base::Time now = base::Time::Now();

  // No result.
  EXPECT_TRUE(metadata_utils::HasExpiredOrUnavailableResult(segment_info, now));

  // Unexpired result.
  auto* prediction_result = segment_info.mutable_prediction_result();
  base::Time result_time = base::Time::Now() - base::Days(3);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  prediction_result->add_result(1);
  EXPECT_FALSE(
      metadata_utils::HasExpiredOrUnavailableResult(segment_info, now));

  // Expired result.
  result_time = base::Time::Now() - base::Days(30);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(metadata_utils::HasExpiredOrUnavailableResult(segment_info, now));
}

TEST_F(MetadataUtilsTest, GetTimeUnit) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::TimeUnit::DAY);
  EXPECT_EQ(base::Days(1), metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::HOUR);
  EXPECT_EQ(base::Hours(1), metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::MINUTE);
  EXPECT_EQ(base::Minutes(1), metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::SECOND);
  EXPECT_EQ(base::Seconds(1), metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::WEEK);
  EXPECT_EQ(base::Days(7), metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::MONTH);
  EXPECT_EQ(base::Days(30), metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::YEAR);
  EXPECT_EQ(base::Days(365), metadata_utils::GetTimeUnit(metadata));
}

TEST_F(MetadataUtilsTest, SignalTypeToSignalKind) {
  EXPECT_EQ(
      SignalKey::Kind::USER_ACTION,
      metadata_utils::SignalTypeToSignalKind(proto::SignalType::USER_ACTION));
  EXPECT_EQ(SignalKey::Kind::HISTOGRAM_ENUM,
            metadata_utils::SignalTypeToSignalKind(
                proto::SignalType::HISTOGRAM_ENUM));
  EXPECT_EQ(SignalKey::Kind::HISTOGRAM_VALUE,
            metadata_utils::SignalTypeToSignalKind(
                proto::SignalType::HISTOGRAM_VALUE));
  EXPECT_EQ(SignalKey::Kind::UNKNOWN,
            metadata_utils::SignalTypeToSignalKind(
                proto::SignalType::UNKNOWN_SIGNAL_TYPE));
}

TEST_F(MetadataUtilsTest, SignalKindToSignalType) {
  EXPECT_EQ(
      proto::SignalType::USER_ACTION,
      metadata_utils::SignalKindToSignalType(SignalKey::Kind::USER_ACTION));
  EXPECT_EQ(
      proto::SignalType::HISTOGRAM_ENUM,
      metadata_utils::SignalKindToSignalType(SignalKey::Kind::HISTOGRAM_ENUM));
  EXPECT_EQ(
      proto::SignalType::HISTOGRAM_VALUE,
      metadata_utils::SignalKindToSignalType(SignalKey::Kind::HISTOGRAM_VALUE));
  EXPECT_EQ(proto::SignalType::UNKNOWN_SIGNAL_TYPE,
            metadata_utils::SignalKindToSignalType(SignalKey::Kind::UNKNOWN));
}

TEST_F(MetadataUtilsTest, CheckDiscreteMapping) {
  proto::SegmentationModelMetadata metadata;
  std::string segmentation_key = "some_key";
  float mapping[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  AddDiscreteMapping(&metadata, mapping, 3, segmentation_key);

  ASSERT_EQ(0, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.1,
                                                      metadata));
  ASSERT_EQ(1, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.4,
                                                      metadata));
  ASSERT_EQ(3, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.5,
                                                      metadata));
  ASSERT_EQ(3, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.6,
                                                      metadata));
  ASSERT_EQ(4, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.9,
                                                      metadata));
}

TEST_F(MetadataUtilsTest, CheckDiscreteMappingInNonAscendingOrder) {
  proto::SegmentationModelMetadata metadata;
  std::string segmentation_key = "some_key";
  float mapping[][2] = {{0.2, 1}, {0.7, 4}, {0.5, 3}};
  AddDiscreteMapping(&metadata, mapping, 3, segmentation_key);

  ASSERT_EQ(0, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.1,
                                                      metadata));
  ASSERT_EQ(1, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.4,
                                                      metadata));
  ASSERT_EQ(3, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.5,
                                                      metadata));
  ASSERT_EQ(3, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.6,
                                                      metadata));
  ASSERT_EQ(4, metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.9,
                                                      metadata));
}

TEST_F(MetadataUtilsTest, CheckMissingDiscreteMapping) {
  proto::SegmentationModelMetadata metadata;
  std::string segmentation_key = "some_key";

  // Any value should result in a 0 mapping, since no mapping exists.
  ASSERT_NEAR(
      0.9,
      metadata_utils::ConvertToDiscreteScore(segmentation_key, 0.9, metadata),
      0.01);
}

TEST_F(MetadataUtilsTest, CheckDefaultDiscreteMapping) {
  std::string segmentation_key = "some_key";
  float mapping_specific[][2] = {{0.2, 1}, {0.5, 3}, {0.7, 4}};
  float mapping_default[][2] = {{0.2, 5}, {0.5, 6}, {0.7, 7}};
  proto::SegmentationModelMetadata metadata;
  AddDiscreteMapping(&metadata, mapping_specific, 3, segmentation_key);
  AddDiscreteMapping(&metadata, mapping_default, 3, "my-default");

  // No valid mapping should be found since there is no default mapping, returns
  // the score.
  EXPECT_NEAR(
      0.6,
      metadata_utils::ConvertToDiscreteScore("non-existing-key", 0.6, metadata),
      0.01);

  metadata.set_default_discrete_mapping("my-default");
  // Should now use the default values instead of the one from the
  // one in the configuration key.
  EXPECT_NEAR(
      6,
      metadata_utils::ConvertToDiscreteScore("non-existing-key", 0.6, metadata),
      0.01);
}

TEST_F(MetadataUtilsTest, CheckMissingDefaultDiscreteMapping) {
  proto::SegmentationModelMetadata metadata;
  std::string segmentation_key = "some_key";
  float mapping_default[][2] = {{0.2, 5}, {0.5, 6}, {0.7, 7}};
  AddDiscreteMapping(&metadata, mapping_default, 3, "my-default");
  metadata.set_default_discrete_mapping("not-my-default");

  // Should not find 'not-my-default' mapping, since it is registered as
  // 'my-default', so we should get the score as default value.
  EXPECT_NEAR(
      0.6,
      metadata_utils::ConvertToDiscreteScore("non-existing-key", 0.6, metadata),
      0.01);
}

TEST_F(MetadataUtilsTest, SegmetationModelMetadataToString) {
  proto::SegmentationModelMetadata metadata;
  ASSERT_TRUE(
      metadata_utils::SegmetationModelMetadataToString(metadata).empty());

  proto::UMAFeature feature;
  feature.set_type(proto::SignalType::UNKNOWN_SIGNAL_TYPE);
  feature.set_name("test name");
  feature.set_aggregation(proto::Aggregation::COUNT);
  feature.set_bucket_count(456);
  *metadata.add_features() = feature;

  std::string expected =
      "feature:{type:UNKNOWN_SIGNAL_TYPE, name:test name, bucket_count:456, "
      "aggregation:COUNT}";
  ASSERT_EQ(metadata_utils::SegmetationModelMetadataToString(metadata),
            expected);

  metadata.set_bucket_duration(10);
  metadata.set_min_signal_collection_length(7);
  ASSERT_EQ(metadata_utils::SegmetationModelMetadataToString(metadata),
            expected + ", bucket_duration:10, min_signal_collection_length:7");
}

TEST_F(MetadataUtilsTest, GetAllUmaFeatures) {
  proto::SegmentationModelMetadata model_metadata;

  proto::UMAFeature feature1;
  feature1.set_name("feature1");
  *model_metadata.add_features() = feature1;

  proto::UMAFeature feature2;
  feature2.set_name("feature2");
  *model_metadata.add_features() = feature2;

  std::vector<proto::UMAFeature> expected = metadata_utils::GetAllUmaFeatures(
      model_metadata, /*include_outputs=*/false);

  ASSERT_EQ(model_metadata.features_size(), (int)expected.size());

  for (int i = 0; i < model_metadata.features_size(); ++i) {
    ASSERT_EQ(model_metadata.features(i).name(), expected[i].name());
  }
}

TEST_F(MetadataUtilsTest, GetAllUmaFeaturesWithInputFeatures) {
  proto::SegmentationModelMetadata model_metadata;

  // Adds two inputs.
  auto* input1 = model_metadata.add_input_features();
  proto::UMAFeature* feature1 = input1->mutable_uma_feature();
  feature1->set_name("feature1");

  auto* input2 = model_metadata.add_input_features();
  proto::UMAFeature* feature2 = input2->mutable_uma_feature();
  feature2->set_name("feature2");

  // Adds one output.
  auto* output = model_metadata.mutable_training_outputs()
                     ->add_outputs()
                     ->mutable_uma_output()
                     ->mutable_uma_feature();
  output->set_name("output");

  std::vector<proto::UMAFeature> expected = metadata_utils::GetAllUmaFeatures(
      model_metadata, /*include_outputs=*/false);

  // Verifies that only all the inputs are included.
  ASSERT_EQ(model_metadata.input_features_size(), (int)expected.size());
  for (int i = 0; i < model_metadata.input_features_size(); ++i) {
    if (model_metadata.input_features(i).has_uma_feature()) {
      ASSERT_EQ(model_metadata.input_features(i).uma_feature().name(),
                expected[i].name());
    }
  }
}

TEST_F(MetadataUtilsTest, GetAllUmaFeaturesWithUMAOutput) {
  proto::SegmentationModelMetadata model_metadata;
  auto* uma_feature = model_metadata.mutable_training_outputs()
                          ->add_outputs()
                          ->mutable_uma_output()
                          ->mutable_uma_feature();
  uma_feature->set_name("output");

  std::vector<proto::UMAFeature> expected = metadata_utils::GetAllUmaFeatures(
      model_metadata, /*include_outputs=*/true);
  EXPECT_EQ(1u, expected.size());
  EXPECT_EQ("output", expected[0].name());
}

TEST_F(MetadataUtilsTest, ConfigUsesLegacyOutput) {
  auto config = test_utils::CreateTestConfig(
      "test_key", SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER);
  EXPECT_FALSE(metadata_utils::ConfigUsesLegacyOutput(config.get()));

  config = test_utils::CreateTestConfig(
      "test_key", SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER);
  EXPECT_FALSE(metadata_utils::ConfigUsesLegacyOutput(config.get()));
}

}  // namespace segmentation_platform
