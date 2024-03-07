// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/migration_test_utils.h"

#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::migration_test_utils {

constexpr std::array<const char*, 5> kAdaptiveToolbarModelLabels = {
    kAdaptiveToolbarModelLabelNewTab, kAdaptiveToolbarModelLabelShare,
    kAdaptiveToolbarModelLabelVoice, kAdaptiveToolbarModelLabelTranslate,
    kAdaptiveToolbarModelLabelAddToBookmarks};

std::unique_ptr<Config> GetTestConfigForBinaryClassifier(
    const std::string& segmentation_key,
    const std::string& segmentation_uma_name,
    proto::SegmentId segment_id) {
  auto config = std::make_unique<Config>();
  config->segmentation_key = segmentation_key;
  config->segmentation_uma_name = segmentation_uma_name;
  config->AddSegmentId(segment_id);
  config->segment_selection_ttl = base::Days(7);
  config->unknown_selection_ttl = base::Days(7);
  config->is_boolean_segment = true;
  return config;
}

std::unique_ptr<Config> GetTestConfigForAdaptiveToolbar() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;
  config->segmentation_uma_name = kAdaptiveToolbarUmaName;

  config->segment_selection_ttl = base::Days(7);

  config->AddSegmentId(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  config->AddSegmentId(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  config->AddSegmentId(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
  return config;
}

proto::OutputConfig GetTestOutputConfigForBinaryClassifier(
    proto::SegmentId segment_id) {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5,
      /*positive_label=*/SegmentIdToHistogramVariant(segment_id),
      /*negative_label=*/"Other");
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/7,
      /*time_unit=*/proto::TimeUnit::DAY);
  return model_metadata.output_config();
}

proto::OutputConfig GetTestOutputConfigForAdaptiveToolbar() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForMultiClassClassifier(kAdaptiveToolbarModelLabels,
                                                /*top_k_outputs=*/1,
                                                /*threshold=*/1);

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/7,
      /*time_unit=*/proto::TimeUnit::DAY);

  return model_metadata.output_config();
}

}  // namespace segmentation_platform::migration_test_utils
