// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_

#include <cinttypes>
#include <cstddef>

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

// Utility to write metadata proto for default models.
class MetadataWriter {
 public:
  explicit MetadataWriter(proto::SegmentationModelMetadata* metadata);
  ~MetadataWriter();

  MetadataWriter(const MetadataWriter&) = delete;
  MetadataWriter& operator=(const MetadataWriter&) = delete;

  // Defines a feature based on UMA metric.
  struct UMAFeature {
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
  // TODO(ssid): Support custom inputs.
  struct SqlFeature {
    const char* const sql{nullptr};
    struct EventAndMetrics {
      const UkmEventHash event_hash;
      const raw_ptr<const UkmMetricHash> metrics{nullptr};
      const size_t metrics_size{0};
    };
    const raw_ptr<const EventAndMetrics> events{nullptr};
    const size_t events_size{0};
  };

  // Defines a feature based on a custom input.
  struct CustomInput {
    const uint64_t tensor_length{0};
    const proto::CustomInput::FillPolicy fill_policy{
        proto::CustomInput_FillPolicy_UNKNOWN_FILL_POLICY};
    const size_t default_values_size{0};
    const raw_ptr<const float> default_values = nullptr;
    const char* name{nullptr};
  };

  // Appends the list of UMA features in order.
  void AddUmaFeatures(const UMAFeature features[],
                      size_t features_size,
                      bool is_output = false);

  // Appends the list of SQL features in order.
  void AddSqlFeatures(const SqlFeature features[], size_t features_size);

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
  void AddOutputConfigForMultiClassClassifier(const char* const* class_labels,
                                              size_t class_labels_length,
                                              int top_k_outputs,
                                              absl::optional<float> threshold);

  // Adds a BinnedClassifier.
  void AddOutputConfigForBinnedClassifier(
      const std::vector<std::pair<float, std::string>>& bins,
      std::string underflow_label);

  // Adds a `PredictedResultTTL` in `OutputConfig`.
  void AddPredictedResultTTLInOutputConfig(
      std::vector<std::pair<std::string, std::int64_t>> top_label_to_ttl_list,
      int64_t default_ttl,
      proto::TimeUnit time_unit);

  // Append a delay trigger for training data collection.
  void AddDelayTrigger(uint64_t delay_sec);

 private:
  const raw_ptr<proto::SegmentationModelMetadata> metadata_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
