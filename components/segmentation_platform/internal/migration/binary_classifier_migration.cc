// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/binary_classifier_migration.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform::pref_migration_utils {

namespace {

proto::OutputConfig CreateOutputConfigForBinaryClassifier(Config* config) {
  DCHECK(config->segments.size() == 1);
  proto::SegmentId segment_id = config->segments.begin()->first;
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5,
      /*positive_label=*/config->GetSegmentUmaName(segment_id),
      /*negative_label=*/"Other");
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/config->segment_selection_ttl / base::Days(1),
      /*time_unit=*/proto::TimeUnit::DAY);
  return model_metadata.output_config();
}

std::vector<float> PopulateModelScoresForBinaryClassifier(
    Config* config,
    const SelectedSegment& old_result) {
  float model_score =
      (config->segments.begin()->first == old_result.segment_id) ? 1 : 0;
  return std::vector<float>(1, model_score);
}

}  // namespace

proto::ClientResult CreateClientResultForBinaryClassifier(
    Config* config,
    const SelectedSegment& old_result) {
  const proto::OutputConfig& output_config =
      CreateOutputConfigForBinaryClassifier(config);

  std::vector<float> model_scores =
      PopulateModelScoresForBinaryClassifier(config, old_result);

  proto::PredictionResult pred_result = metadata_utils::CreatePredictionResult(
      model_scores, output_config, /*timestamp=*/base::Time::Now(),
      /*model_version=*/1);

  return metadata_utils::CreateClientResultFromPredResult(
      pred_result, old_result.selection_time);
}

}  // namespace segmentation_platform::pref_migration_utils
