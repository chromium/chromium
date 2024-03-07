// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"

#include <array>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {

enum class DeviceSwitcherClass {
  kAndroidPhone = 0,
  kIosPhoneChrome = 1,
  kAndroidTablet = 2,
  kIosTablet = 3,
  kDesktop = 4,
  kOther = 5,
  kSyncedAndFirstDevice = 6,
  kNotSynced = 7,
  kMaxValue = kNotSynced
};

constexpr std::array<const char*, 8> kOutputLabels = {
    DeviceSwitcherModel::kAndroidPhoneLabel,
    DeviceSwitcherModel::kIosPhoneChromeLabel,
    DeviceSwitcherModel::kAndroidTabletLabel,
    DeviceSwitcherModel::kIosTabletLabel,
    DeviceSwitcherModel::kDesktopLabel,
    DeviceSwitcherModel::kOtherLabel,
    DeviceSwitcherModel::kSyncedAndFirstDeviceLabel,
    DeviceSwitcherModel::kNotSyncedLabel};

static_assert(kOutputLabels.size() == (int)DeviceSwitcherClass::kMaxValue + 1,
              "labels size must be same as the classes");
#define RANK(x) static_cast<int>(x)

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kDeviceSwitcherModelId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER;
constexpr int64_t kDeviceSwitcherMinSignalCollectionLength = 0;

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

  auto* sync_input = writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 10,
      .fill_policy = proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      .name = "SyncDeviceInfo"});
  (*sync_input->mutable_additional_args())["wait_for_device_info_in_seconds"] =
      "60";

  writer.AddOutputConfigForMultiClassClassifier(kOutputLabels,
                                                kOutputLabels.size(), 0.1);

  constexpr int kModelVersion = 1;
  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void DeviceSwitcherModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // The custom input added should return 10 float values.
  if (inputs.size() != 10) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response result(kOutputLabels.size(), 0);

  if (inputs[0] != 0) {
    // Inputs failed to fetch from sync.
    result[RANK(DeviceSwitcherClass::kNotSynced)] = 1;
  } else {
    // Order the labels based on the count of devices and additionally increase
    // by a priority factor to break ties.
    result[RANK(DeviceSwitcherClass::kAndroidPhone)] = inputs[1] * 1.10;
    result[RANK(DeviceSwitcherClass::kIosPhoneChrome)] = inputs[3] * 1.09;
    result[RANK(DeviceSwitcherClass::kAndroidTablet)] = inputs[2] * 1.08;
    result[RANK(DeviceSwitcherClass::kIosTablet)] = inputs[4] * 1.07;
    result[RANK(DeviceSwitcherClass::kDesktop)] =
        (inputs[5] + inputs[6] + inputs[7] + inputs[8]) * 1.06;
    result[RANK(DeviceSwitcherClass::kOther)] = inputs[9] * 1.05;

    int total = 0;
    for (unsigned i = 1; i < 10; ++i) {
      total += inputs[i];
    }
    result[RANK(DeviceSwitcherClass::kSyncedAndFirstDevice)] =
        total == 0 ? 1 : 0;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace segmentation_platform
