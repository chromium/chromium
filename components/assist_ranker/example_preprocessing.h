// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_EXAMPLE_PREPROCESSING_H_
#define COMPONENTS_ASSIST_RANKER_EXAMPLE_PREPROCESSING_H_

#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "third_party/protobuf/src/google/protobuf/map.h"

namespace assist_ranker {

// Preprocessor for preprocessing RankerExample into formats that is needed by
// Ranker Predictors.
class ExamplePreprocessor {
 public:
  // Error code (bitwise) for preprocessing.
  enum PreprocessErrorCode {
    kSuccess = 0,
    kNoFeatureIndexFound = 1,
    kNonbucketizableFeatureType = 2,
    kInvalidFeatureType = 4,
    kInvalidFeatureListIndex = 8,
    kNonNormalizableFeatureType = 16,
    kNonConvertibleToStringFeatureType = 32,
    kNormalizerIsZero = 64,
  };

  // Processes a RankerExample with config.
  // Clear up all features except kVectorizedFeatureDefaultName if
  // clear_other_features is set to true.
  // Returns the error code of preprocessing, can be any sum of the error code
  // in PreprocessErrorCode.
  static int Process(const ExamplePreprocessorConfig& config,
                     RankerExample* example,
                     bool clear_other_features = false);

  // Default feature name for missing features.
  static const char kMissingFeatureDefaultName[];

  // Default feature name for vectorized features.
  static const char kVectorizedFeatureDefaultName[];

  // Generates a feature's fullname based on feature_name and feature_value.
  // A feature fullname is defined as:
  //   (1) feature_name if it's bool_value, int64_value or float_value.
  //   (2) a combination of feature_name and feature_value if it's string_value
  //       or i-th element of a string_list.
  static std::string FeatureFullname(const std::string& feature_name,
                                     const std::string& feature_value = "");

 private:
  // If a feature is specified in config.missing_features() and missing in
  // the example, then the feature name is added as a sparse feature value to
  // the special sparse feature "_MissingFeature" in the example.
  // Always returns kSuccess.
  static int AddMissingFeatures(const ExamplePreprocessorConfig& config,
                                RankerExample* example);
  // If a numeric feature is specified in config.bucketizers(), then it is
  // bucketized based on the boundaries and reset as a one-hot feature with
  // bucket index as it's string value.
  static int AddBucketizedFeatures(const ExamplePreprocessorConfig& config,
                                   RankerExample* example);
  // Normalizes numeric features to be within [-1.0, 1.0] as float features.
  static int NormalizeFeatures(const ExamplePreprocessorConfig& config,
                               RankerExample* example);
  // Converts any features in |example| that are listed in
  // |config.convert_to_string_features()| into string-valued features.
  static int ConvertToStringFeatures(const ExamplePreprocessorConfig& config,
                                     RankerExample* example);
  // Add a new_float_list feature as kVectorizedFeatureDefaultName, and iterate
  // for all existing features in example.features(), set corresponding
  // new_float_list.float_value(config.feature_indices(feature_value_key)) to
  // be either numeric value (for scalars) or 1.0 (for string values).
  static int Vectorization(const ExamplePreprocessorConfig& config,
                           RankerExample* example,
                           bool clear_other_features);
};

// An iterator that goes through all features of a RankerExample and converts
// each field as a struct Field{full_name, value, error}.
//   (1) A numeric feature (bool_value, int32_value, float_value) is converted
//       to {feature_name, float(original_value), kSuccess}.
//   (2) A string feature is converted to
//       {feature_name_string_value, 1.0, kSuccess}.
//   (3) A string_value from a string list feature is converted to
//       {feature_name_string_value, 1.0, error_code} where non-empty list
//       gets error_code kSuccess, empty list gets kInvalidFeatureListIndex.
// Example:
//   std::vector<float> ExampleToStdFloat(const RankerExample& example,
//                                        const Map& feature_indices) {
//     std::vector<float> vectorized(feature_indices.size());
//     for (const auto& field : ExampleFloatIterator(example)) {
//       if (field.error == ExamplePreprocessor::kSuccess) {
//         const int index = feature_indices[field.fullname];
//         vectorized[index] = field.value;
//       }
//     }
//     return vectorized;
//   }
class ExampleFloatIterator {
 public:
  // A struct as float value of one field from a RankerExample.
  struct Field {
    std::string fullname;
    float value;
    int error;
  };

  explicit ExampleFloatIterator(const RankerExample& example)
      : feature_iterator_(example.features().begin()),
        feature_end_iterator_(example.features().end()),
        string_list_index_(0) {}

  ExampleFloatIterator begin() const { return *this; }
  ExampleFloatIterator end() const {
    return ExampleFloatIterator(feature_end_iterator_);
  }

  Field operator*() const;

  ExampleFloatIterator& operator++();

  // Two iterators are equal if they point to the same field, with the same
  // indices if it's a string_list.
  bool operator==(const ExampleFloatIterator& other) const {
    return feature_iterator_ == other.feature_iterator_ &&
           string_list_index_ == other.string_list_index_;
  }

  bool operator!=(const ExampleFloatIterator& other) const {
    return !(*this == other);
  }

 private:
  // Returns the end iterator.
  explicit ExampleFloatIterator(
      const google::protobuf::Map<std::string, Feature>::const_iterator&
          feature_end_iterator)
      : feature_iterator_(feature_end_iterator),
        feature_end_iterator_(feature_end_iterator),
        string_list_index_(0) {}

  google::protobuf::Map<std::string, Feature>::const_iterator feature_iterator_;
  google::protobuf::Map<std::string, Feature>::const_iterator
      feature_end_iterator_;
  int string_list_index_;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_EXAMPLE_PREPROCESSING_H_
