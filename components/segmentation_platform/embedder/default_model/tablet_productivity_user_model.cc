// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/tablet_productivity_user_model.h"

#include <array>

#include "base/feature_list.h"
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

// Default parameters for search user model.
constexpr uint64_t kTabletProductivityUserModelVersion = 1;
constexpr SegmentId kTabletProductivityUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_TABLET_PRODUCTIVITY_USER;
constexpr int64_t kTabletProductivityUserSignalStorageLength = 60;
constexpr int64_t kTabletProductivityUserMinSignalCollectionLength = 7;
constexpr int64_t kDefaultTTLInDays = 7;

// InputFeatures.

// Enum values.
constexpr std::array<int32_t, 1> kDefaultDesktopSiteSetting{
    0  // ALLOW.
};

constexpr std::array<int32_t, 3> kWebContentTargetType{0, 1, 2};

// Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 7>
    kTabletProductivityUserUMAFeatures = {
        // 0
        MetadataWriter::UMAFeature::FromValueHistogram(
            "Android.MultiInstance.NumActivities",
            60,
            proto::Aggregation::COUNT),
        // 1
        MetadataWriter::UMAFeature::FromUserAction(
            "Android.MultiWindowMode.MultiInstance.Enter",
            60),
        // 2
        MetadataWriter::UMAFeature::FromEnumHistogram(
            "ContentSettings.RegularProfile.DefaultRequestDesktopSiteSetting",
            60,
            kDefaultDesktopSiteSetting.data(),
            kDefaultDesktopSiteSetting.size()),
        // 3
        MetadataWriter::UMAFeature::FromValueHistogram(
            "Android.ActivityStop.NumberOfTabsUsed",
            60,
            proto::Aggregation::COUNT),
        // 4
        MetadataWriter::UMAFeature::FromValueHistogram(
            "TabGroups.UserGroupCount",
            60,
            proto::Aggregation::COUNT),
        // 5
        MetadataWriter::UMAFeature::FromEnumHistogram(
            "Android.DragDrop.FromWebContent.TargetType",
            60,
            kWebContentTargetType.data(),
            kWebContentTargetType.size()),
        // 6
        MetadataWriter::UMAFeature::FromUserAction(
            "MobilePageLoadedWithKeyboard",
            60),
};

}  // namespace

// static
std::unique_ptr<Config> TabletProductivityUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformTabletProductivityUser)) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kTabletProductivityUserKey;
  config->segmentation_uma_name = kTabletProductivityUserUmaName;
  config->AddSegmentId(kTabletProductivityUserSegmentId,
                       std::make_unique<TabletProductivityUserModel>());
  config->auto_execute_and_cache = true;
  config->is_boolean_segment = true;
  return config;
}

TabletProductivityUserModel::TabletProductivityUserModel()
    : DefaultModelProvider(kTabletProductivityUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
TabletProductivityUserModel::GetModelConfig() {
  proto::SegmentationModelMetadata tablet_productivity_user_metadata;
  MetadataWriter writer(&tablet_productivity_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kTabletProductivityUserMinSignalCollectionLength,
      kTabletProductivityUserSignalStorageLength);

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

  // Set features.
  writer.AddUmaFeatures(kTabletProductivityUserUMAFeatures.data(),
                        kTabletProductivityUserUMAFeatures.size());

  //  Set OutputConfig.
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{0, kTabletProductivityUserModelLabelNone},
                {1, kTabletProductivityUserModelLabelMedium},
                {2, kTabletProductivityUserModelLabelHigh}},
      /*underflow_label=*/kTabletProductivityUserModelLabelNone);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, kDefaultTTLInDays,
      /*time_unit=*/proto::TimeUnit::DAY);

  return std::make_unique<ModelConfig>(
      std::move(tablet_productivity_user_metadata),
      kTabletProductivityUserModelVersion);
}

void TabletProductivityUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() !=
      kTabletProductivityUserUMAFeatures.size() + 3 /*custom_inputs*/) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Getting device tier for user's device.
  int device_tier_score;
  float device_ram_in_gb = inputs[0] / 1024;
  float device_os_version = inputs[1];
  float device_ppi = inputs[2];
  if ((device_ram_in_gb >= 8 && device_os_version >= 10 && device_ppi > 370) ||
      (device_ram_in_gb >= 6 && device_os_version >= 11 && device_ppi > 370)) {
    device_tier_score = 3;  // High-Tier Device
  } else if ((device_ram_in_gb >= 6 &&
              (device_os_version < 11 || device_ppi <= 370)) ||
             (device_ram_in_gb > 2 && device_os_version >= 10)) {
    device_tier_score = 2;  // Medium-Tier Device
  } else if (device_ram_in_gb >= 1 && device_os_version >= 1) {
    device_tier_score = 1;  // Low-Tier Device
  } else {
    device_tier_score = 0;  // None
  }

  int count = 0;

  bool is_multi_instance =
      (inputs[3] > 1 || (device_os_version < 12 && inputs[4] >= 1));
  count += is_multi_instance ? 1 : 0;

  bool desktop_mode_as_default = (inputs[5] >= 1);
  count += desktop_mode_as_default ? 1 : 0;

  bool tab_used = ((inputs[6] > 4) || (inputs[7] >= 1));
  count += tab_used ? 1 : 0;

  bool is_drag_and_drop = (inputs[8] >= 1);
  count += is_drag_and_drop ? 1 : 0;

  bool is_mobile_page_loaded_with_keyboard = (inputs[9] >= 1);
  count += is_mobile_page_loaded_with_keyboard ? 1 : 0;

  // Logic for segmenting users into different productivity group.
  float score = 0;
  if (device_tier_score == 3 && count > 1) {
    score = 2;
  } else if (device_tier_score >= 2 && count >= 1) {
    score = 1;
  } else {
    score = 0;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, score)));
}

}  // namespace segmentation_platform
