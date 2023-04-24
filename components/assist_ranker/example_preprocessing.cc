// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/example_preprocessing.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/assist_ranker/ranker_example_util.h"
#include "third_party/protobuf/src/google/protobuf/map.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace assist_ranker {

using google::protobuf::Map;
using google::protobuf::MapPair;
using google::protobuf::RepeatedField;

// Initialize.
const char ExamplePreprocessor::kMissingFeatureDefaultName[] =
    "_MissingFeature";
const char ExamplePreprocessor::kVectorizedFeatureDefaultName[] =
    "_VectorizedFeature";

std::string ExamplePreprocessor::FeatureFullname(
    const std::string& feature_name,
    const std::string& feature_value) {
  return feature_value.empty()
             ? feature_name
             : base::StrCat({feature_name, "_", feature_value});
}

int ExamplePreprocessor::Process(const ExamplePreprocessorConfig& config,
                                 RankerExample* const example,
                                 const bool clear_other_features) {
  return AddMissingFeatures(config, example) |
         NormalizeFeatures(config, example) |
         AddBucketizedFeatures(config, example) |
         ConvertToStringFeatures(config, example) |
         Vectorization(config, example, clear_other_features);
}

int ExamplePreprocessor::AddMissingFeatures(
    const ExamplePreprocessorConfig& config,
    RankerExample* const example) {
  Map<std::string, Feature>& feature_map = *example->mutable_features();
  for (const std::string& feature_name : config.missing_features()) {
    // If a feature is missing in the example, set the place.
    if (feature_map.find(feature_name) == feature_map.end()) {
      feature_map[kMissingFeatureDefaultName]
          .mutable_string_list()
          ->add_string_value(feature_name);
    }
  }
  return kSuccess;
}

int ExamplePreprocessor::AddBucketizedFeatures(
    const ExamplePreprocessorConfig& config,
    RankerExample* const example) {
  int error_code = kSuccess;
  Map<std::string, Feature>& feature_map = *example->mutable_features();
  for (const MapPair<std::string, ExamplePreprocessorConfig::Boundaries>&
           bucketizer : config.bucketizers()) {
    const std::string& feature_name = bucketizer.first;
    // Simply continue if the feature is missing. The missing feature will later
    // on be handled as missing one_hot feature, and it's up to the user how to
    // handle this missing feature.
    Feature feature;
    if (!SafeGetFeature(feature_name, *example, &feature)) {
      continue;
    }
    // Get feature value as float. Only int32 or float value is supported for
    // Bucketization. Continue if the type_case is not int32 or float.
    float value = 0;
    switch (feature.feature_type_case()) {
      case Feature::kInt32Value:
        value = static_cast<float>(feature.int32_value());
        break;
      case Feature::kFloatValue:
        value = feature.float_value();
        break;
      default:
        DVLOG(2) << "Can't bucketize feature type: "
                 << feature.feature_type_case();
        error_code |= kNonbucketizableFeatureType;
        continue;
    }
    // Get the bucket from the boundaries; the first index that value<boundary.
    const RepeatedField<float>& boundaries = bucketizer.second.boundaries();
    int index = 0;
    for (; index < boundaries.size(); ++index) {
      if (value < boundaries[index])
        break;
    }
    // Set one hot feature as features[feature_name] = "index";
    feature_map[feature_name].set_string_value(base::NumberToString(index));
  }
  return error_code;
}

int ExamplePreprocessor::NormalizeFeatures(
    const ExamplePreprocessorConfig& config,
    RankerExample* example) {
  int error_code = kSuccess;
  for (const MapPair<std::string, float>& pair : config.normalizers()) {
    const std::string& feature_name = pair.first;
    float feature_value = 0.0f;
    if (GetFeatureValueAsFloat(feature_name, *example, &feature_value)) {
      if (pair.second == 0.0f) {
        error_code |= kNormalizerIsZero;
      } else {
        feature_value = feature_value / pair.second;
      }
      // Truncate to be within [-1.0, 1.0].
      feature_value = std::clamp(feature_value, -1.0f, 1.0f);
      (*example->mutable_features())[feature_name].set_float_value(
          feature_value);
    } else {
      error_code |= kNonNormalizableFeatureType;
    }
  }
  return error_code;
}

int ExamplePreprocessor::ConvertToStringFeatures(
    const ExamplePreprocessorConfig& config,
    RankerExample* example) {
  int error_code = kSuccess;
  for (const std::string& feature_name : config.convert_to_string_features()) {
    const auto find_feature = example->mutable_features()->find(feature_name);
    if (find_feature != example->features().end()) {
      auto& feature = find_feature->second;
      switch (feature.feature_type_case()) {
        case Feature::kBoolValue:
          feature.set_string_value(
              base::NumberToString(static_cast<int>(feature.bool_value())));
          break;
        case Feature::kInt32Value:
          feature.set_string_value(base::NumberToString(feature.int32_value()));
          break;
        case Feature::kStringValue:
          break;
        default:
          LOG(WARNING) << "Can't convert to string feature type: "
                       << feature.feature_type_case();
          error_code |= kNonConvertibleToStringFeatureType;
          continue;
      }
    }
  }
  return error_code;
}

int ExamplePreprocessor::Vectorization(const ExamplePreprocessorConfig& config,
                                       RankerExample* example,
                                       const bool clear_other_features) {
  if (config.feature_indices().empty()) {
    DVLOG(2) << "Feature indices are empty, can't vectorize.";
    return kSuccess;
  }
  Feature vectorized_features;
  vectorized_features.mutable_float_list()->mutable_float_value()->Resize(
      config.feature_indices().size(), 0.0);

  int error_code = kSuccess;

  for (const auto& field : ExampleFloatIterator(*example)) {
    error_code |= field.error;
    if (field.error != kSuccess) {
      continue;
    }
    const auto find_index = config.feature_indices().find(field.fullname);
    // If the feature_fullname is inside the indices map, then set the place.
    if (find_index != config.feature_indices().end()) {
      vectorized_features.mutable_float_list()->set_float_value(
          find_index->second, field.value);
    } else {
      DVLOG(2) << "Feature has no index: " << field.fullname;
      error_code |= kNoFeatureIndexFound;
    }
  }
  if (clear_other_features) {
    example->clear_features();
  }
  (*example->mutable_features())[kVectorizedFeatureDefaultName] =
      vectorized_features;
  return error_code;
}

ExampleFloatIterator::Field ExampleFloatIterator::operator*() const {
  const std::string& feature_name = feature_iterator_->first;
  const Feature& feature = feature_iterator_->second;
  Field field = {feature_name, 1.0f, ExamplePreprocessor::kSuccess};

  switch (feature.feature_type_case()) {
    case Feature::kBoolValue:
      field.value = static_cast<float>(feature.bool_value());
      break;
    case Feature::kInt32Value:
      field.value = static_cast<float>(feature.int32_value());
      break;
    case Feature::kFloatValue:
      field.value = feature.float_value();
      break;
    case Feature::kStringValue:
      field.fullname = ExamplePreprocessor::FeatureFullname(
          feature_name, feature.string_value());
      break;
    case Feature::kStringList:
      if (string_list_index_ < feature.string_list().string_value_size()) {
        const std::string& string_value =
            feature.string_list().string_value(string_list_index_);
        field.fullname =
            ExamplePreprocessor::FeatureFullname(feature_name, string_value);
      } else {
        // This happens when a string list field is added without any value.
        field.error = ExamplePreprocessor::kInvalidFeatureListIndex;
      }
      break;
    default:
      field.error = ExamplePreprocessor::kInvalidFeatureType;
      DVLOG(2) << "Feature type not supported:  "
               << feature.feature_type_case();
      break;
  }
  return field;
}

ExampleFloatIterator& ExampleFloatIterator::operator++() {
  const Feature& feature = feature_iterator_->second;
  switch (feature.feature_type_case()) {
    case Feature::kBoolValue:
    case Feature::kInt32Value:
    case Feature::kFloatValue:
    case Feature::kStringValue:
      ++feature_iterator_;
      break;
    case Feature::kStringList:
      if (string_list_index_ < feature.string_list().string_value_size() - 1) {
        // If not at the last element, advance the index.
        ++string_list_index_;
      } else {
        // If at the last element, advance the feature_iterator.
        string_list_index_ = 0;
        ++feature_iterator_;
      }
      break;
    default:
      ++feature_iterator_;
      DVLOG(2) << "Feature type not supported:  "
               << feature.feature_type_case();
  }
  return *this;
}

}  // namespace assist_ranker
