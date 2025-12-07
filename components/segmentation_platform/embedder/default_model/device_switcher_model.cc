// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"

#include <array>
#include <vector>

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

// Default parameters for Chrome Start model.
constexpr SegmentId kDeviceSwitcherModelId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER;
constexpr int64_t kDeviceSwitcherMinSignalCollectionLength = 0;

constexpr LabelPair<DeviceSwitcherModel::Label> kDeviceSwitcherLabels[] = {
    {DeviceSwitcherModel::kLabelAndroidPhone,
     DeviceSwitcherModel::kAndroidPhoneLabel},
    {DeviceSwitcherModel::kLabelIosPhoneChrome,
     DeviceSwitcherModel::kIosPhoneChromeLabel},
    {DeviceSwitcherModel::kLabelAndroidTablet,
     DeviceSwitcherModel::kAndroidTabletLabel},
    {DeviceSwitcherModel::kLabelIosTablet,
     DeviceSwitcherModel::kIosTabletLabel},
    {DeviceSwitcherModel::kLabelDesktop, DeviceSwitcherModel::kDesktopLabel},
    {DeviceSwitcherModel::kLabelOther, DeviceSwitcherModel::kOtherLabel},
    {DeviceSwitcherModel::kLabelSyncedAndFirstDevice,
     DeviceSwitcherModel::kSyncedAndFirstDeviceLabel},
    {DeviceSwitcherModel::kLabelNotSynced,
     DeviceSwitcherModel::kNotSyncedLabel}};

constexpr FeaturePair<DeviceSwitcherModel::Feature> kDeviceSwitcherFeatures[] =
    {std::make_pair(
        // Since this feature has tensor length > 1, just use the first index.
        DeviceSwitcherModel::kFeatureSyncSuccess,
        features::Feature::FromCustomInput(features::CustomInput{
            .tensor_length = 10,
            .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
            .name = "SyncDeviceInfo"}))};

}  // namespace

// static
std::unique_ptr<Config> DeviceSwitcherModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformDeviceSwitcher)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kDeviceSwitcherKey;
  config->segmentation_uma_name = kDeviceSwitcherUmaName;
  config->AddSegmentId(kDeviceSwitcherModelId,
                       std::make_unique<DeviceSwitcherModel>());
  config->is_boolean_segment = false;
  config->auto_execute_and_cache = false;
  return config;
}

DeviceSwitcherModel::DeviceSwitcherModel()
    : DefaultModelProvider(kDeviceSwitcherModelId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
DeviceSwitcherModel::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kDeviceSwitcherMinSignalCollectionLength);
  metadata.set_upload_tensors(false);
  metadata.set_return_type(
      proto::SegmentationModelMetadata::RETURN_TYPE_MULTISEGMENT);

  writer.AddFeatures<Feature>(kDeviceSwitcherFeatures);

  // TODO(ssid): Add an API to define additional args in the const feature list.
  auto* sync_input = metadata.mutable_input_features(0)->mutable_custom_input();
  (*sync_input->mutable_additional_args())["wait_for_device_info_in_seconds"] =
      "60";

  writer.AddOutputConfigForMultiClassClassifier(
      base::span<const LabelPair<DeviceSwitcherModel::Label>>(
          kDeviceSwitcherLabels),
      0.1);

  constexpr int kModelVersion = 1;
  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void DeviceSwitcherModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // The custom input added should return `kFeatureCount` float values.
  if (inputs.size() != kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response result(kLabelCount, 0);

  if (inputs[kFeatureSyncSuccess] != 0) {
    // Inputs failed to fetch from sync.
    result[kLabelNotSynced] = 1;
  } else {
    // Order the labels based on the count of devices and additionally increase
    // by a priority factor to break ties.
    result[kLabelAndroidPhone] = inputs[kFeatureAndroidPhoneCount] * 1.10;
    result[kLabelIosPhoneChrome] = inputs[kFeatureIosPhoneCount] * 1.09;
    result[kLabelAndroidTablet] = inputs[kFeatureAndroidTabletCount] * 1.08;
    result[kLabelIosTablet] = inputs[kFeatureIosTabletCount] * 1.07;
    result[kLabelDesktop] =
        (inputs[kFeatureLinuxCount] + inputs[kFeatureMacCount] +
         inputs[kFeatureWindowsCount] + inputs[kFeatureChromeOsCount]) *
        1.06;
    result[kLabelOther] = inputs[kFeatureOtherCount] * 1.05;

    int total = 0;
    for (unsigned i = kFeatureAndroidPhoneCount; i < kFeatureCount; ++i) {
      total += inputs[i];
    }
    result[kLabelSyncedAndFirstDevice] = total == 0 ? 1 : 0;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace segmentation_platform
