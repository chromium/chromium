// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_tier_segment.h"

#include <array>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for device tier model.
constexpr uint64_t kDeviceTierSegmentVersion = 1;
constexpr SegmentId kDeviceTierSegmentId = SegmentId::DEVICE_TIER_SEGMENT;
constexpr int64_t kDeviceTierSegmentSignalStorageLength = 28;
constexpr int64_t kDeviceTierSegmentMinSignalCollectionLength = 0;
}  // namespace

// static
std::unique_ptr<Config> DeviceTierSegment::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformDeviceTier)) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kDeviceTierKey;
  config->segmentation_uma_name = kDeviceTierUmaName;
  config->auto_execute_and_cache = true;
  config->AddSegmentId(kDeviceTierSegmentId,
                       std::make_unique<DeviceTierSegment>());
  config->is_boolean_segment = true;
  return config;
}

DeviceTierSegment::DeviceTierSegment()
    : DefaultModelProvider(kDeviceTierSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
DeviceTierSegment::GetModelConfig() {
  proto::SegmentationModelMetadata device_tier_metadata;
  MetadataWriter writer(&device_tier_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kDeviceTierSegmentMinSignalCollectionLength,
      kDeviceTierSegmentSignalStorageLength);

  // Adding custom inputs.
  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_DEVICE_RAM_MB,
      .name = "DeviceRAMInMB"});

  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_DEVICE_OS_VERSION_NUMBER,
      .name = "DeviceOSVersionNumber"});

  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_DEVICE_PPI,
      .name = "DevicePPI"});

  //  Set OutputConfig.
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{1, kDeviceTierSegmentLabelLow},
                {2, kDeviceTierSegmentLabelMedium},
                {3, kDeviceTierSegmentLabelHigh}},
      /*underflow_label=*/kDeviceTierSegmentLabelNone);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/7,
      /*time_unit=*/proto::TimeUnit::DAY);

  return std::make_unique<ModelConfig>(std::move(device_tier_metadata),
                                       kDeviceTierSegmentVersion);
}

void DeviceTierSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 3) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  float score = 0;
  float device_ram_in_gb = inputs[0] / 1024;
  float device_os_version = inputs[1];
  float device_ppi = inputs[2];
  if ((device_ram_in_gb >= 8 && device_os_version >= 10 && device_ppi > 370) ||
      (device_ram_in_gb >= 6 && device_os_version >= 11 && device_ppi > 370)) {
    score = 3;
  } else if ((device_ram_in_gb >= 6 &&
              (device_os_version < 11 || device_ppi <= 370)) ||
             (device_ram_in_gb > 2 && device_os_version >= 10)) {
    score = 2;
  } else if (device_ram_in_gb >= 1 && device_os_version >= 1) {
    score = 1;
  } else {
    score = 0;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, score)));
}

}  // namespace segmentation_platform
