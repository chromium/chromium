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
    kAndroidPhoneLabel,
    kIosPhoneChromeLabel,
    kAndroidTabletLabel,
    kIosTabletLabel,
    kDesktopLabel,
    kOtherLabel,
    kSyncedAndFirstDeviceLabel,
    kNotSyncedLabel};

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
  auto config = std::make_unique<Config>();
  config->segmentation_key = kDeviceSwitcherKey;
  config->segmentation_uma_name = kDeviceSwitcherUmaName;
  config->AddSegmentId(kDeviceSwitcherModelId,
                       std::make_unique<DeviceSwitcherModel>());
  config->is_boolean_segment = false;
  config->on_demand_execution = true;
  return config;
}

DeviceSwitcherModel::DeviceSwitcherModel()
    : ModelProvider(kDeviceSwitcherModelId) {}

void DeviceSwitcherModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
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
      "5";
  (*sync_input->mutable_additional_args())["active_days_limit"] = "14";

  writer.AddOutputConfigForMultiClassClassifier(
      kOutputLabels.begin(), kOutputLabels.size(), kOutputLabels.size(), 0.1);

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kDeviceSwitcherModelId,
                          std::move(metadata), kModelVersion));
}

void DeviceSwitcherModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // The custom input added should return 10 float values.
  if (inputs.size() != 10) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  ModelProvider::Response result(kOutputLabels.size(), 0);

  if (inputs[0] != 0) {
    // Inputs failed to fetch from sync.
    result[RANK(DeviceSwitcherClass::kNotSynced)] = 1;
  } else {
    // Assign a priority to the labels for the result. 0 labels will not be
    // returned to the client.
    result[RANK(DeviceSwitcherClass::kAndroidPhone)] = inputs[1] >= 1 ? 10 : 0;
    result[RANK(DeviceSwitcherClass::kIosPhoneChrome)] = inputs[3] >= 1 ? 9 : 0;
    result[RANK(DeviceSwitcherClass::kAndroidTablet)] = inputs[2] >= 1 ? 8 : 0;
    result[RANK(DeviceSwitcherClass::kIosTablet)] = inputs[4] >= 1 ? 7 : 0;
    result[RANK(DeviceSwitcherClass::kDesktop)] =
        (inputs[5] + inputs[6] + inputs[7] + inputs[8]) >= 1 ? 6 : 0;
    result[RANK(DeviceSwitcherClass::kOther)] = inputs[9] >= 1 ? 3 : 0;

    int total = 0;
    for (unsigned i = 1; i < 10; ++i) {
      total += inputs[i];
    }
    result[RANK(DeviceSwitcherClass::kSyncedAndFirstDevice)] =
        total == 0 ? 2 : 0;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

bool DeviceSwitcherModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
