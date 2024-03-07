// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/adaptive_toolbar_migration.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform::pref_migration_utils {

namespace {

constexpr std::array<const char*, 5> kAdaptiveToolbarModelLabels = {
    kAdaptiveToolbarModelLabelNewTab, kAdaptiveToolbarModelLabelShare,
    kAdaptiveToolbarModelLabelVoice, kAdaptiveToolbarModelLabelTranslate,
    kAdaptiveToolbarModelLabelAddToBookmarks};

proto::OutputConfig CreateOutputConfigForAdaptiveToolbar(Config* config) {
  DCHECK(config->segments.size() >= 1);
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForMultiClassClassifier(kAdaptiveToolbarModelLabels,
                                                /*top_k_outputs=*/1,
                                                /*threshold=*/1);

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/config->segment_selection_ttl / base::Days(1),
      /*time_unit=*/proto::TimeUnit::DAY);

  return model_metadata.output_config();
}

std::vector<float> PopulateModelScoresForAdaptiveToolbar(
    Config* config,
    const SelectedSegment& old_result) {
  std::vector<float> model_scores = {0, 0, 0, 0, 0};
  proto::SegmentId segment_id = old_result.segment_id;
  switch (segment_id) {
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      model_scores[0] = 1;
      break;
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      model_scores[1] = 1;
      break;
    case SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      model_scores[2] = 1;
      break;
    default:  // Unknown
      break;
  }
  return model_scores;
}

}  // namespace

proto::ClientResult CreateClientResultForAdaptiveToolbar(
    Config* config,
    const SelectedSegment& old_result) {
  proto::ClientResult client_result;
  const proto::OutputConfig& output_config =
      CreateOutputConfigForAdaptiveToolbar(config);
  std::vector<float> model_scores =
      PopulateModelScoresForAdaptiveToolbar(config, old_result);
  proto::PredictionResult pred_result = metadata_utils::CreatePredictionResult(
      model_scores, output_config, /*timestamp=*/base::Time::Now(),
      /*model_version=*/1);
  return metadata_utils::CreateClientResultFromPredResult(
      pred_result, old_result.selection_time);
}

}  // namespace segmentation_platform::pref_migration_utils
