// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/example_preprocessing.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/map.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace assist_ranker {
namespace {

using ::google::protobuf::Map;
using ::google::protobuf::RepeatedField;

void EXPECT_EQUALS_EXAMPLE(const RankerExample& example1,
                           const RankerExample& example2) {
  EXPECT_EQ(example1.features_size(), example2.features_size());
  for (const auto& pair : example1.features()) {
    const Feature& feature1 = pair.second;
    const Feature& feature2 = example2.features().at(pair.first);
    EXPECT_EQ(feature1.feature_type_case(), feature2.feature_type_case());
    EXPECT_EQ(feature1.bool_value(), feature2.bool_value());
    EXPECT_EQ(feature1.int32_value(), feature2.int32_value());
    EXPECT_EQ(feature1.float_value(), feature2.float_value());
    EXPECT_EQ(feature1.string_value(), feature2.string_value());
    EXPECT_EQ(feature1.string_list().string_value_size(),
              feature2.string_list().string_value_size());
    for (int i = 0; i < feature1.string_list().string_value_size(); ++i) {
      EXPECT_EQ(feature1.string_list().string_value(i),
                feature2.string_list().string_value(i));
    }
  }
}

}  // namespace

class ExamplePreprocessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto& features = *example_.mutable_features();
    features[bool_name_].set_bool_value(bool_value_);
    features[int32_name_].set_int32_value(int32_value_);
    features[float_name_].set_float_value(float_value_);
    features[one_hot_name_].set_string_value(one_hot_value_);
    *features[sparse_name_].mutable_string_list()->mutable_string_value() = {
        sparse_values_.begin(), sparse_values_.end()};
  }

  RankerExample example_;
  const std::string bool_name_ = "bool_feature";
  const bool bool_value_ = true;
  const std::string int32_name_ = "int32_feature";
  const int int32_value_ = 2;
  const std::string float_name_ = "float_feature";
  const float float_value_ = 3.0;
  const std::string one_hot_name_ = "one_hot_feature";
  const std::string elem1_ = "elem1";
  const std::string elem2_ = "elem2";
  const std::string one_hot_value_ = elem1_;
  const std::string sparse_name_ = "sparse_feature";
  const std::vector<std::string> sparse_values_ = {elem1_, elem2_};
};

TEST_F(ExamplePreprocessorTest, AddMissingFeatures) {
  RankerExample expected = example_;
  ExamplePreprocessorConfig config;

  // Adding missing feature label to an existing feature has no effect.
  config.add_missing_features(bool_name_);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();

  // Adding missing feature label to non-existing feature returns a
  // "_MissingFeature" feature with a list of feature names.
  const std::string foo = "foo";
  config.add_missing_features(foo);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  (*expected
        .mutable_features())[ExamplePreprocessor::kMissingFeatureDefaultName]
      .mutable_string_list()
      ->add_string_value(foo);
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();
}

TEST_F(ExamplePreprocessorTest, AddBucketizeFeatures) {
  RankerExample expected = example_;
  ExamplePreprocessorConfig config;
  Map<std::string, ExamplePreprocessorConfig::Boundaries>& bucketizers =
      *config.mutable_bucketizers();

  // Adding bucketized feature to non-existing feature returns the same example.
  const std::string foo = "foo";
  bucketizers[foo].add_boundaries(0.5);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();

  // Bucketizing a bool feature returns same proto.
  bucketizers[bool_name_].add_boundaries(0.5);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kNonbucketizableFeatureType);
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();

  // Bucketizing a string feature returns same proto.
  bucketizers[one_hot_name_].add_boundaries(0.5);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kNonbucketizableFeatureType);
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();

  // Bucketizing an int32 feature with 3 boundary.
  bucketizers[int32_name_].add_boundaries(int32_value_ - 2);
  bucketizers[int32_name_].add_boundaries(int32_value_ - 1);
  bucketizers[int32_name_].add_boundaries(int32_value_ + 1);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  (*expected.mutable_features())[int32_name_].set_string_value("2");
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();

  // Bucketizing a float feature with 3 boundary.
  bucketizers[float_name_].add_boundaries(float_value_ - 0.2);
  bucketizers[float_name_].add_boundaries(float_value_ - 0.1);
  bucketizers[float_name_].add_boundaries(float_value_ + 0.1);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  (*expected.mutable_features())[float_name_].set_string_value("2");
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();

  // Bucketizing a float feature with value equal to a boundary.
  (*example_.mutable_features())[float_name_].set_float_value(float_value_);
  bucketizers[float_name_].add_boundaries(float_value_ - 0.2);
  bucketizers[float_name_].add_boundaries(float_value_ - 0.1);
  bucketizers[float_name_].add_boundaries(float_value_);
  bucketizers[float_name_].add_boundaries(float_value_ + 0.1);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  (*expected.mutable_features())[float_name_].set_string_value("3");
  EXPECT_EQUALS_EXAMPLE(example_, expected);
  config.Clear();
}

// Tests normalization of float and int32 features.
TEST_F(ExamplePreprocessorTest, NormalizeFeatures) {
  RankerExample expected = example_;
  ExamplePreprocessorConfig config;
  Map<std::string, float>& normalizers = *config.mutable_normalizers();
  normalizers[int32_name_] = int32_value_ - 1.0f;
  normalizers[float_name_] = float_value_ + 1.0f;

  (*expected.mutable_features())[int32_name_].set_float_value(1.0f);
  (*expected.mutable_features())[float_name_].set_float_value(
      float_value_ / (float_value_ + 1.0f));

  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);
  EXPECT_EQUALS_EXAMPLE(example_, expected);

  // Zero normalizer returns an error.
  normalizers[float_name_] = 0.0f;
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kNormalizerIsZero);
}

// Zero normalizer returns an error.
TEST_F(ExamplePreprocessorTest, ZeroNormalizerReturnsError) {
  ExamplePreprocessorConfig config;
  (*config.mutable_normalizers())[float_name_] = 0.0f;
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kNormalizerIsZero);
}

// Tests converts a bool or int32 feature to a string feature.
TEST_F(ExamplePreprocessorTest, ConvertToStringFeatures) {
  RankerExample expected = example_;
  ExamplePreprocessorConfig config;
  auto& features_list = *config.mutable_convert_to_string_features();
  *features_list.Add() = bool_name_;
  *features_list.Add() = int32_name_;
  *features_list.Add() = one_hot_name_;

  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kSuccess);

  (*expected.mutable_features())[bool_name_].set_string_value(
      base::NumberToString(static_cast<int>(bool_value_)));
  (*expected.mutable_features())[int32_name_].set_string_value(
      base::NumberToString(int32_value_));
  EXPECT_EQUALS_EXAMPLE(example_, expected);
}

// Float features can't be convert to string features.
TEST_F(ExamplePreprocessorTest,
       ConvertFloatFeatureToStringFeatureReturnsError) {
  ExamplePreprocessorConfig config;
  config.add_convert_to_string_features(float_name_);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_),
            ExamplePreprocessor::kNonConvertibleToStringFeatureType);
}

TEST_F(ExamplePreprocessorTest, Vectorization) {
  ExamplePreprocessorConfig config;
  Map<std::string, int32_t>& feature_indices =
      *config.mutable_feature_indices();

  RankerExample example_vec_expected = example_;
  RepeatedField<float>& feature_vector =
      *(*example_vec_expected.mutable_features())
           [ExamplePreprocessor::kVectorizedFeatureDefaultName]
               .mutable_float_list()
               ->mutable_float_value();

  // bool feature puts the value to the corresponding place.
  feature_indices[bool_name_] = 0;
  feature_vector.Add(1.0);

  // int32 feature puts the value to the corresponding place.
  feature_indices[int32_name_] = 1;
  feature_vector.Add(int32_value_);

  // float feature puts the value to the corresponding place.
  feature_indices[float_name_] = 2;
  feature_vector.Add(float_value_);

  // string value is vectorized as 1.0.
  feature_indices[ExamplePreprocessor::FeatureFullname(one_hot_name_,
                                                       one_hot_value_)] = 3;
  feature_vector.Add(1.0);

  // string list value is vectorized as 1.0.
  feature_indices[ExamplePreprocessor::FeatureFullname(sparse_name_, elem1_)] =
      4;
  feature_indices[ExamplePreprocessor::FeatureFullname(sparse_name_, elem2_)] =
      5;
  feature_vector.Add(1.0);
  feature_vector.Add(1.0);

  // string list value with element not in the example sets the corresponding
  // place as 0.0;
  feature_indices[ExamplePreprocessor::FeatureFullname(sparse_name_, "foo")] =
      5;
  feature_vector.Add(0.0);

  // Non-existing feature puts 0 to the corresponding place.
  feature_indices["bar"] = 6;
  feature_vector.Add(0.0);

  // Verify the propressing result.
  RankerExample example = example_;
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example),
            ExamplePreprocessor::kSuccess);
  EXPECT_EQUALS_EXAMPLE(example, example_vec_expected);

  // Example with extra numeric feature gets kNoFeatureIndexFound error;
  RankerExample example_with_extra_numeric = example_;
  (*example_with_extra_numeric.mutable_features())["foo"].set_float_value(1.0);
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_with_extra_numeric),
            ExamplePreprocessor::ExamplePreprocessor::kNoFeatureIndexFound);

  // Example with extra one-hot feature gets kNoFeatureIndexFound error;
  RankerExample example_with_extra_one_hot = example_;
  (*example_with_extra_one_hot.mutable_features())["foo"].set_string_value(
      "bar");
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_with_extra_one_hot),
            ExamplePreprocessor::ExamplePreprocessor::kNoFeatureIndexFound);

  // Example with extra sparse feature value gets kNoFeatureIndexFound error;
  RankerExample example_with_extra_sparse = example_;
  (*example_with_extra_sparse.mutable_features())[sparse_name_]
      .mutable_string_list()
      ->add_string_value("bar");
  EXPECT_EQ(ExamplePreprocessor::Process(config, &example_with_extra_sparse),
            ExamplePreprocessor::ExamplePreprocessor::kNoFeatureIndexFound);
}

TEST_F(ExamplePreprocessorTest, MultipleErrorCode) {
  ExamplePreprocessorConfig config;

  (*config.mutable_feature_indices())[int32_name_] = 0;
  (*config.mutable_feature_indices())[float_name_] = 1;
  (*config.mutable_bucketizers())[one_hot_name_].add_boundaries(0.5);
  RankerExample example_vec_expected = example_;
  RepeatedField<float>& feature_vector =
      *(*example_vec_expected.mutable_features())
           [ExamplePreprocessor::kVectorizedFeatureDefaultName]
               .mutable_float_list()
               ->mutable_float_value();

  feature_vector.Add(int32_value_);
  feature_vector.Add(float_value_);

  const int error_code = ExamplePreprocessor::Process(config, &example_);
  // Error code contains features in example_ but not in feature_indices.
  EXPECT_TRUE(error_code & ExamplePreprocessor::kNoFeatureIndexFound);
  // Error code contains features that are not bucketizable.
  EXPECT_TRUE(error_code & ExamplePreprocessor::kNonbucketizableFeatureType);
  // No kInvalidFeatureType error.
  EXPECT_FALSE(error_code & ExamplePreprocessor::kInvalidFeatureType);
  // Only two elements is correctly vectorized.
  EXPECT_EQUALS_EXAMPLE(example_, example_vec_expected);
}

TEST_F(ExamplePreprocessorTest, ExampleFloatIterator) {
  RankerExample float_example;
  for (const auto& field : ExampleFloatIterator(example_)) {
    EXPECT_EQ(field.error, ExamplePreprocessor::kSuccess);
    (*float_example.mutable_features())[field.fullname].set_float_value(
        field.value);
  }

  RankerExample float_example_expected;
  auto& feature_map = *float_example_expected.mutable_features();

  feature_map[bool_name_].set_float_value(bool_value_);
  feature_map[int32_name_].set_float_value(int32_value_);
  feature_map[float_name_].set_float_value(float_value_);
  feature_map[ExamplePreprocessor::FeatureFullname(one_hot_name_,
                                                   one_hot_value_)]
      .set_float_value(1.0);
  feature_map[ExamplePreprocessor::FeatureFullname(sparse_name_, elem1_)]
      .set_float_value(1.0);
  feature_map[ExamplePreprocessor::FeatureFullname(sparse_name_, elem2_)]
      .set_float_value(1.0);

  EXPECT_EQUALS_EXAMPLE(float_example, float_example_expected);
}

TEST_F(ExamplePreprocessorTest, ExampleFloatIteratorError) {
  RankerExample example;
  example.mutable_features()->insert({"foo", Feature::default_instance()});
  (*example.mutable_features())["bar"]
      .mutable_string_list()
      ->mutable_string_value();
  int num_of_fields = 0;
  for (const auto& field : ExampleFloatIterator(example)) {
    if (field.fullname == "foo") {
      EXPECT_EQ(field.error, ExamplePreprocessor::kInvalidFeatureType);
    }
    if (field.fullname == "bar") {
      EXPECT_EQ(field.error, ExamplePreprocessor::kInvalidFeatureListIndex);
    }
    ++num_of_fields;
  }
  // Check the iterator indeed found the two fields.
  EXPECT_EQ(num_of_fields, 2);
}

}  // namespace assist_ranker
