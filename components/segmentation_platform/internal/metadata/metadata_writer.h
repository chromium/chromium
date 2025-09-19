// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_

#include <cinttypes>
#include <cstddef>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

// TODO(ssid): This file should be moved to public API of segmentation and
// macros should be separated from this header.

// The following `GENERATE` macros are helpers for `DEFINE_FEATURES` and
// `DEFINE_LABELS` macros. They are used to generate enums and arrays from a
// list of features or labels. The `feature_list` or `label_list` is expected to
// be a macro that takes another macro as an argument, and applies it to each
// feature/label.

// Generates an enum member from a feature definition.
#define SEGMENTATION_INTERNAL_GENERATE_ENUM(name, feature) name,
// Generates a feature struct from a feature definition.
#define SEGMENTATION_INTERNAL_GENERATE_FEATURE(name, feature) feature,
// Generates an enum member from a label definition.
#define SEGMENTATION_INTERNAL_GENERATE_LABEL_ENUM(name, label_string) name,
// Generates a label string from a label definition.
#define SEGMENTATION_INTERNAL_GENERATE_LABEL_STRING(name, label_string) \
  label_string,

// Public helper macros:

// A helper macro to define a single UMA feature based on a user action.
#define SEGMENTATION_USER_ACTION(uma_name, days) \
  MetadataWriter::Feature::FromUMAFeature(       \
      MetadataWriter::UMAFeature::FromUserAction(uma_name, days))

// A helper macro to define a single UMA feature based on an enum histogram.
#define SEGMENTATION_UMA_ENUM(uma_name, days, enum_ids, enum_size)            \
  MetadataWriter::Feature::FromUMAFeature(                                    \
      MetadataWriter::UMAFeature::FromEnumHistogram(uma_name, days, enum_ids, \
                                                    enum_size))

// A helper macro to define a custom input feature.
#define SEGMENTATION_INPUT_CONTEXT(input_name)                          \
  MetadataWriter::Feature::FromCustomInput(MetadataWriter::CustomInput{ \
      .tensor_length = 1,                                               \
      .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,       \
      .name = input_name})

// A helper macro to define a list of features. This macro generates an enum for
// feature indices and a constexpr array of `MetadataWriter::Feature` structs.
#define SEGMENTATION_DEFINE_FEATURES(array_name, feature_list)       \
  enum { feature_list(SEGMENTATION_INTERNAL_GENERATE_ENUM) kCount }; \
  inline constexpr MetadataWriter::Feature array_name[] = {          \
      feature_list(SEGMENTATION_INTERNAL_GENERATE_FEATURE)}

// A helper macro to define a list of output labels. This macro generates an
// enum for label indices and a constexpr array of label strings.
#define SEGMENTATION_DEFINE_LABELS(array_name, label_list)                     \
  enum { label_list(SEGMENTATION_INTERNAL_GENERATE_LABEL_ENUM) kLabelsCount }; \
  inline constexpr const char* array_name[] = {                                \
      label_list(SEGMENTATION_INTERNAL_GENERATE_LABEL_STRING)}

// Utility to write metadata proto for default models.
class MetadataWriter {
 public:
  explicit MetadataWriter(proto::SegmentationModelMetadata* metadata);
  ~MetadataWriter();

  MetadataWriter(const MetadataWriter&) = delete;
  MetadataWriter& operator=(const MetadataWriter&) = delete;

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
    const size_t default_values_size{0};
    const float* const default_values = nullptr;

    static constexpr UMAFeature FromUserAction(const char* name,
                                               uint64_t bucket_count) {
      return MetadataWriter::UMAFeature{
          .signal_type = proto::SignalType::USER_ACTION,
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
      return MetadataWriter::UMAFeature{
          .signal_type = proto::SignalType::HISTOGRAM_VALUE,
          .name = name,
          .bucket_count = bucket_count,
          .tensor_length = 1,
          .aggregation = aggregation,
          .enum_ids_size = 0,
          .default_values_size = default_values_size,
          .default_values = default_values};
    }

    static constexpr UMAFeature FromEnumHistogram(const char* name,
                                                  uint64_t bucket_count,
                                                  const int32_t* const enum_ids,
                                                  size_t enum_ids_size) {
      return MetadataWriter::UMAFeature{
          .signal_type = proto::SignalType::HISTOGRAM_ENUM,
          .name = name,
          .bucket_count = bucket_count,
          .tensor_length = 1,
          .aggregation = proto::Aggregation::COUNT,
          .enum_ids_size = enum_ids_size,
          .accepted_enum_ids = enum_ids};
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
    const size_t default_values_size{0};
    const float* const default_values = nullptr;
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
    static constexpr Feature FromUMAFeature(
        MetadataWriter::UMAFeature uma_feature) {
      return Feature{.uma_feature = uma_feature};
    }
    static constexpr Feature FromSqlFeature(
        MetadataWriter::SqlFeature sql_feature) {
      return Feature{.sql_feature = sql_feature};
    }
    static constexpr Feature FromCustomInput(
        MetadataWriter::CustomInput custom_input) {
      return Feature{.custom_input = custom_input};
    }

    const std::optional<MetadataWriter::UMAFeature> uma_feature;
    const std::optional<MetadataWriter::SqlFeature> sql_feature;
    const std::optional<MetadataWriter::CustomInput> custom_input;
  };

  // Appends the list of UMA features in order.
  void AddUmaFeatures(const UMAFeature features[],
                      size_t features_size,
                      bool is_output = false);

  // Appends the list of features in order.
  void AddFeatures(const base::span<const Feature> features);

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
