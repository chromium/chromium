// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/metadata/metadata_writer.h"

#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

MetadataWriter::MetadataWriter(proto::SegmentationModelMetadata* metadata)
    : metadata_(metadata) {}
MetadataWriter::~MetadataWriter() = default;

void MetadataWriter::AddUmaFeatures(const UMAFeature features[],
                                    size_t features_size) {
  for (size_t i = 0; i < features_size; i++) {
    const auto& feature = features[i];
    auto* input_feature = metadata_->add_input_features();
    proto::UMAFeature* uma_feature = input_feature->mutable_uma_feature();
    uma_feature->set_type(feature.signal_type);
    uma_feature->set_name(feature.name);
    uma_feature->set_name_hash(base::HashMetricName(feature.name));
    uma_feature->set_bucket_count(feature.bucket_count);
    uma_feature->set_tensor_length(feature.tensor_length);
    uma_feature->set_aggregation(feature.aggregation);

    for (size_t j = 0; j < feature.enum_ids_size; j++) {
      uma_feature->add_enum_ids(feature.accepted_enum_ids[j]);
    }

    for (size_t j = 0; j < feature.default_values_size; j++) {
      uma_feature->add_default_values(feature.default_values[j]);
    }
  }
}

void MetadataWriter::AddSqlFeatures(const SqlFeature features[],
                                    size_t features_size) {
  proto::SqlFeature* feature =
      metadata_->add_input_features()->mutable_sql_feature();
  for (size_t i = 0; i < features_size; ++i) {
    const auto& f = features[i];
    feature->set_sql(f.sql);
    for (size_t ev = 0; ev < f.events_size; ++ev) {
      const auto& event = f.events[ev];
      auto* ukm_event = feature->mutable_signal_filter()->add_ukm_events();
      ukm_event->set_event_hash(event.event_hash.GetUnsafeValue());
      for (size_t m = 0; m < event.metrics_size; ++m) {
        ukm_event->mutable_metric_hash_filter()->Add(
            event.metrics[m].GetUnsafeValue());
      }
    }
  }
}

proto::CustomInput* MetadataWriter::AddCustomInput(const CustomInput& feature) {
  proto::CustomInput* custom_input_feature =
      metadata_->add_input_features()->mutable_custom_input();
  custom_input_feature->set_tensor_length(feature.tensor_length);
  custom_input_feature->set_fill_policy(feature.fill_policy);
  custom_input_feature->add_default_value(feature.default_value);
  custom_input_feature->set_name(feature.name);
  return custom_input_feature;
}

void MetadataWriter::AddDiscreteMappingEntries(
    const std::string& key,
    const std::pair<float, int>* mappings,
    size_t mappings_size) {
  auto* discrete_mappings = metadata_->mutable_discrete_mappings();
  for (size_t i = 0; i < mappings_size; i++) {
    auto* discrete_mapping_entry = (*discrete_mappings)[key].add_entries();
    discrete_mapping_entry->set_min_result(mappings[i].first);
    discrete_mapping_entry->set_rank(mappings[i].second);
  }
}

void MetadataWriter::AddBooleanSegmentDiscreteMapping(const std::string& key) {
  const int selected_rank = 1;
  const float model_score = 1;
  const std::pair<float, int> mappings[]{{model_score, selected_rank}};
  AddDiscreteMappingEntries(key, mappings, 1);
}

void MetadataWriter::AddBooleanSegmentDiscreteMappingWithSubsegments(
    const std::string& key,
    float threshold,
    int max_value) {
  DCHECK_GT(threshold, 0);
  // Should record at least 2 subsegments.
  DCHECK_GT(max_value, 1);
  const int selected_rank = 1;
  const std::pair<float, int> mappings[]{{threshold, selected_rank}};
  AddDiscreteMappingEntries(key, mappings, 1);

  std::vector<std::pair<float, int>> subsegment_mapping;
  for (int i = 1; i <= max_value; ++i) {
    subsegment_mapping.emplace_back(i, i);
  }
  AddDiscreteMappingEntries(
      base::StrCat({key, kSubsegmentDiscreteMappingSuffix}),
      subsegment_mapping.data(), subsegment_mapping.size());
}

void MetadataWriter::SetSegmentationMetadataConfig(
    proto::TimeUnit time_unit,
    uint64_t bucket_duration,
    int64_t signal_storage_length,
    int64_t min_signal_collection_length,
    int64_t result_time_to_live) {
  metadata_->set_time_unit(time_unit);
  metadata_->set_bucket_duration(bucket_duration);
  metadata_->set_signal_storage_length(signal_storage_length);
  metadata_->set_min_signal_collection_length(min_signal_collection_length);
  metadata_->set_result_time_to_live(result_time_to_live);
}

void MetadataWriter::SetDefaultSegmentationMetadataConfig(
    int min_signal_collection_length_days,
    int signal_storage_length_days) {
  SetSegmentationMetadataConfig(proto::TimeUnit::DAY, /*bucket_duration=*/1,
                                signal_storage_length_days,
                                min_signal_collection_length_days,
                                /*result_time_to_live=*/1);
}

void MetadataWriter::AddOutputConfigForBinaryClassifier(
    float threshold,
    const std::string& positive_label,
    const std::string& negative_label) {
  proto::Predictor_BinaryClassifier* binary_classifier =
      metadata_->mutable_output_config()
          ->mutable_predictor()
          ->mutable_binary_classifier();

  binary_classifier->set_threshold(threshold);
  binary_classifier->set_positive_label(positive_label);
  binary_classifier->set_negative_label(negative_label);
}

void MetadataWriter::AddOutputConfigForMultiClassClassifier(
    const std::vector<std::string>& class_labels,
    int top_k_outputs,
    absl::optional<float> threshold) {
  proto::Predictor_MultiClassClassifier* multi_class_classifier =
      metadata_->mutable_output_config()
          ->mutable_predictor()
          ->mutable_multi_class_classifier();

  multi_class_classifier->set_top_k_outputs(top_k_outputs);
  multi_class_classifier->mutable_class_labels()->Assign(class_labels.begin(),
                                                         class_labels.end());
  if (threshold.has_value()) {
    multi_class_classifier->set_threshold(threshold.value());
  }
}

void MetadataWriter::AddOutputConfigForBinnedClassifier(
    const std::vector<std::pair<float, std::string>>& bins,
    std::string underflow_label) {
  proto::Predictor_BinnedClassifier* binned_classifier =
      metadata_->mutable_output_config()
          ->mutable_predictor()
          ->mutable_binned_classifier();

  binned_classifier->set_underflow_label(underflow_label);
  for (const std::pair<float, std::string>& bin : bins) {
    proto::Predictor::BinnedClassifier::Bin* current_bin =
        binned_classifier->add_bins();
    current_bin->set_min_range(bin.first);
    current_bin->set_label(bin.second);
  }
}

void MetadataWriter::AddDelayTrigger(uint64_t delay_sec) {
  auto* config =
      metadata_->mutable_training_outputs()->mutable_trigger_config();
  auto* trigger = config->add_observation_trigger();
  trigger->set_delay_sec(delay_sec);
  config->set_decision_type(proto::TrainingOutputs::TriggerConfig::ONDEMAND);
}

}  // namespace segmentation_platform
