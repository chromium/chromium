// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_

#include <array>
#include <cinttypes>
#include <cstddef>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

// TODO(ssid): This file should be moved to public API of segmentation.

namespace segmentation_platform {

// LINT.IfChange
namespace features {

// Defines default values for a feature.
struct DefaultValue {
  STACK_ALLOCATED();

 public:
  enum class Type { kNotSet, kSingle, kSpan, kArray };

  union Value {
    constexpr Value() = default;
    constexpr explicit Value(float v) : single_value(v) {}
    constexpr explicit Value(base::span<const float> s) : span_value(s) {}
    struct ArrayValue {
      const float* ptr{nullptr};
      size_t size{0};
    };
    constexpr Value(const float* p, size_t s) : array_value({p, s}) {}

    float single_value{0};
    base::span<const float> span_value;
    ArrayValue array_value;
  };

  const Type type{Type::kNotSet};
  const Value value;

  constexpr DefaultValue() = default;
  constexpr explicit DefaultValue(float v) : type(Type::kSingle), value(v) {}
  constexpr explicit DefaultValue(base::span<const float> s)
      : type(Type::kSpan), value(s) {}
  constexpr DefaultValue(const float* p, size_t s)
      : type(Type::kArray), value(p, s) {}
};

// Defines a feature based on UMA metric.
struct UMAFeature {
  STACK_ALLOCATED();

 public:
  const proto::SignalType signal_type{proto::SignalType::UNKNOWN_SIGNAL_TYPE};
  const char* name{nullptr};
  const uint64_t bucket_count{0};
  const uint64_t tensor_length{0};
  const proto::Aggregation aggregation{proto::Aggregation::UNKNOWN};
  const size_t enum_ids_size{0};
  const int32_t* const accepted_enum_ids = nullptr;
  const DefaultValue default_value;

  static constexpr UMAFeature FromUserAction(const char* name,
                                             uint64_t bucket_count) {
    return UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                      .name = name,
                      .bucket_count = bucket_count,
                      .tensor_length = 1,
                      .aggregation = proto::Aggregation::COUNT,
                      .enum_ids_size = 0};
  }

  static constexpr UMAFeature FromValueHistogram(
      const char* name,
      uint64_t bucket_count,
      proto::Aggregation aggregation,
      size_t default_values_size = 0,
      const float* const default_values = nullptr) {
    return UMAFeature{.signal_type = proto::SignalType::HISTOGRAM_VALUE,
                      .name = name,
                      .bucket_count = bucket_count,
                      .tensor_length = 1,
                      .aggregation = aggregation,
                      .enum_ids_size = 0,
                      .default_value = {default_values, default_values_size}};
  }

  static constexpr UMAFeature FromEnumHistogram(const char* name,
                                                uint64_t bucket_count,
                                                const int32_t* const enum_ids,
                                                size_t enum_ids_size) {
    return UMAFeature{.signal_type = proto::SignalType::HISTOGRAM_ENUM,
                      .name = name,
                      .bucket_count = bucket_count,
                      .tensor_length = 1,
                      .aggregation = proto::Aggregation::COUNT,
                      .enum_ids_size = enum_ids_size,
                      .accepted_enum_ids = enum_ids};
  }

  static constexpr UMAFeature FromEnumHistogram(
      const char* name,
      uint64_t bucket_count,
      base::span<const int32_t> enum_ids) {
    return UMAFeature{.signal_type = proto::SignalType::HISTOGRAM_ENUM,
                      .name = name,
                      .bucket_count = bucket_count,
                      .tensor_length = 1,
                      .aggregation = proto::Aggregation::COUNT,
                      .enum_ids_size = enum_ids.size(),
                      .accepted_enum_ids = enum_ids.data()};
  }

  static constexpr UMAFeature FromLatestOrDefaultValue(
      const char* name,
      uint64_t bucket_count,
      float default_value_float) {
    return UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_VALUE,
        .name = name,
        .bucket_count = bucket_count,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::LATEST_OR_DEFAULT,
        .default_value = DefaultValue(default_value_float),
    };
  }
};

// Defines a feature based on a SQL query.
struct SqlFeature {
  STACK_ALLOCATED();

 public:
  const char* const sql{nullptr};
  struct EventAndMetrics {
    STACK_ALLOCATED();

   public:
    const UkmEventHash event_hash;
    const UkmMetricHash* const metrics = nullptr;
    const size_t metrics_size{0};
  };
  const EventAndMetrics* const events = nullptr;
  const size_t events_size{0};
};

// Defines a feature based on a custom input.
struct CustomInput {
  STACK_ALLOCATED();

 public:
  const uint64_t tensor_length{0};
  const proto::CustomInput::FillPolicy fill_policy{
      proto::CustomInput_FillPolicy_UNKNOWN_FILL_POLICY};
  const DefaultValue default_value;
  const char* name{nullptr};

  using Arg = std::pair<const char*, const char*>;
  const Arg* arg{nullptr};
  const size_t arg_size{0};
};

using BindValueType = proto::SqlFeature::BindValue::ParamType;
using BindValue = std::pair<BindValueType, CustomInput>;
using BindValues = std::vector<BindValue>;

// Defines a feature that can be one of UMA, SQL or CustomInput.
struct Feature {
  STACK_ALLOCATED();

 public:
  static constexpr Feature FromUMAFeature(UMAFeature uma_feature) {
    return Feature{.uma_feature = uma_feature};
  }
  static constexpr Feature FromSqlFeature(SqlFeature sql_feature) {
    return Feature{.sql_feature = sql_feature};
  }
  static constexpr Feature FromCustomInput(CustomInput custom_input) {
    return Feature{.custom_input = custom_input};
  }

  const std::optional<UMAFeature> uma_feature;
  const std::optional<SqlFeature> sql_feature;
  const std::optional<CustomInput> custom_input;
};

constexpr Feature UserAction(const char* name, uint64_t bucket_count) {
  return Feature::FromUMAFeature(
      UMAFeature::FromUserAction(name, bucket_count));
}

template <size_t N>
constexpr Feature UMAEnum(const char* name,
                          uint64_t bucket_count,
                          const std::array<const int32_t, N>& enum_ids) {
  return Feature::FromUMAFeature(
      UMAFeature::FromEnumHistogram(name, bucket_count, enum_ids));
}

constexpr Feature UMAEnum(const char* name,
                          uint64_t bucket_count,
                          base::span<const int32_t> enum_ids) {
  return Feature::FromUMAFeature(
      UMAFeature::FromEnumHistogram(name, bucket_count, enum_ids));
}

constexpr Feature UMACount(const char* name, uint64_t bucket_count) {
  return Feature::FromUMAFeature(UMAFeature::FromValueHistogram(
      name, bucket_count, proto::Aggregation::COUNT));
}

constexpr Feature InputContext(const char* name) {
  return Feature::FromCustomInput(
      CustomInput{.tensor_length = 1,
                  .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
                  .name = name});
}

constexpr Feature LatestOrDefaultValue(const char* name,
                                       uint64_t bucket_count,
                                       float default_value) {
  return Feature::FromUMAFeature(
      UMAFeature::FromLatestOrDefaultValue(name, bucket_count, default_value));
}

constexpr Feature UMASum(const char* name, uint64_t bucket_count) {
  return Feature::FromUMAFeature(
      UMAFeature{.signal_type = proto::SignalType::HISTOGRAM_VALUE,
                 .name = name,
                 .bucket_count = bucket_count,
                 .tensor_length = 1,
                 .aggregation = proto::Aggregation::SUM});
}

constexpr Feature UMAAggregate(const char* name,
                               uint64_t bucket_count,
                               proto::Aggregation aggregation) {
  return Feature::FromUMAFeature(
      UMAFeature{.signal_type = proto::SignalType::HISTOGRAM_VALUE,
                 .name = name,
                 .bucket_count = bucket_count,
                 .tensor_length = 1,
                 .aggregation = aggregation});
}

}  // namespace features
// LINT.ThenChange(//components/segmentation_platform/tools/generate_histogram_list.py)

template <typename EnumType>
using FeaturePair = std::pair<EnumType, const features::Feature>;

template <typename EnumType>
using FeatureSet = base::span<const FeaturePair<EnumType>>;

template <typename EnumType>
using LabelPair = std::pair<EnumType, const char*>;

template <typename EnumType>
using LabelSet = base::span<const LabelPair<EnumType>>;

// Utility to write metadata proto for default models.
class MetadataWriter {
 public:
  // DEPRECATED: Use features::helper to create features.
  using UMAFeature = features::UMAFeature;
  using SqlFeature = features::SqlFeature;
  using CustomInput = features::CustomInput;
  using Feature = features::Feature;
  using BindValueType = features::BindValueType;
  using BindValue = features::BindValue;
  using BindValues = features::BindValues;

  explicit MetadataWriter(proto::SegmentationModelMetadata* metadata);
  ~MetadataWriter();

  MetadataWriter(const MetadataWriter&) = delete;
  MetadataWriter& operator=(const MetadataWriter&) = delete;

  // Appends the list of UMA features in order.
  void AddUmaFeatures(const UMAFeature features[],
                      size_t features_size,
                      bool is_output = false);

  // Appends the list of features in order.
  void AddFeatures(const base::span<const Feature> features);

  template <typename FeatureEnum>
  void AddFeatures(const FeatureSet<FeatureEnum> features_set) {
    for (const auto& pair : features_set) {
      AddFeatures({pair.second});
    }
  }

  // Appends the list of SQL features in order.
  proto::SqlFeature* AddSqlFeature(const SqlFeature& feature);

  proto::SqlFeature* AddSqlFeature(const SqlFeature& feature,
                                   const BindValues& bind_values);

  void AddBindValueToSql(proto::SqlFeature* sql_feature);

  // Creates a custom input feature and appeands to the list of custom inputs in
  // order.
  proto::CustomInput* AddCustomInput(const CustomInput& feature);

  // Appends a list of discrete mapping in order.
  void AddDiscreteMappingEntries(const std::string& key,
                                 const std::pair<float, int>* mappings,
                                 size_t mappings_size);

  // Appends a boolean segmentation mapping, where the model returns 1 or 0 for
  // segment selection.
  void AddBooleanSegmentDiscreteMapping(const std::string& key);

  // Appends a boolean mapping and a subsegment mapping. Set the threshold to
  // the cutoff segment value, and for any value strictly less than `threshold`,
  // then the selection will return no. The `max_value` is set to the max enum
  // value returned by the model.
  void AddBooleanSegmentDiscreteMappingWithSubsegments(const std::string& key,
                                                       float threshold,
                                                       int max_value);

  // Writes the model metadata with the given parameters.
  void SetSegmentationMetadataConfig(proto::TimeUnit time_unit,
                                     uint64_t bucket_duration,
                                     int64_t signal_storage_length,
                                     int64_t min_signal_collection_length,
                                     int64_t result_time_to_live);

  // Uses default setting for model metadata using DAY time unit and 1 day
  // buckets.
  void SetDefaultSegmentationMetadataConfig(
      int min_signal_collection_length_days = 7,
      int signal_storage_length_days = 28);

  // Adds a BinaryClassifier.
  void AddOutputConfigForBinaryClassifier(float threshold,
                                          const std::string& positive_label,
                                          const std::string& negative_label);

  // Adds a MultiClassClassifier.
  void AddOutputConfigForMultiClassClassifier(
      base::span<const char* const> class_labels,
      int top_k_outputs,
      std::optional<float> threshold);
  void AddOutputConfigForMultiClassClassifier(
      const std::vector<std::string>& class_labels,
      int top_k_outputs,
      std::optional<float> threshold);

  template <typename LabelEnum>
  void AddOutputConfigForMultiClassClassifier(
      const LabelSet<LabelEnum> class_labels_set,
      std::optional<float> threshold) {
    std::vector<const char*> class_labels_vector;
    class_labels_vector.reserve(class_labels_set.size());
    for (const auto& pair : class_labels_set) {
      class_labels_vector.push_back(pair.second);
    }
    AddOutputConfigForMultiClassClassifier(
        base::span<const char* const>(class_labels_vector),
        class_labels_vector.size(), threshold);
  }

  // Adds a MultiClassClassifier with one threshold per label.
  void AddOutputConfigForMultiClassClassifier(
      base::span<const char* const> class_labels,
      int top_k_outputs,
      const base::span<float> per_label_thresholds);

  // Adds a BinnedClassifier.
  void AddOutputConfigForBinnedClassifier(
      const std::vector<std::pair<float, std::string>>& bins,
      std::string underflow_label);

  // Adds a generic predictor output config.
  void AddOutputConfigForGenericPredictor(
      const std::vector<std::string>& labels);

  // Adds a `PredictedResultTTL` in `OutputConfig`.
  void AddPredictedResultTTLInOutputConfig(
      std::vector<std::pair<std::string, std::int64_t>> top_label_to_ttl_list,
      int64_t default_ttl,
      proto::TimeUnit time_unit);

  // Sets `ignore_previous_model_ttl` as true in `OutputConfig`.
  void SetIgnorePreviousModelTTLInOutputConfig();

  // Append a delay trigger for training data collection.
  void AddDelayTrigger(uint64_t delay_sec);

  // Adds a custom input from Input Context.
  void AddFromInputContext(const char* custom_input_name,
                           const char* additional_args_name);

 private:
  const raw_ptr<proto::SegmentationModelMetadata> metadata_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
