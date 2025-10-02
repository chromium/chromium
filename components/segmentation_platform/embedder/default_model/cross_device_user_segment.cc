// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"

#include <array>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "ui/base/device_form_factor.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

// Default parameters for cross device model.
constexpr int kModelVersion = 2;
constexpr SegmentId kCrossDeviceUserSegmentId =
    SegmentId::CROSS_DEVICE_USER_SEGMENT;
constexpr int64_t kCrossDeviceUserSignalStorageLength = 28;
constexpr int64_t kCrossDeviceUserMinSignalCollectionLength = 1;
constexpr int kCrossDeviceUserSegmentSelectionTTLDays = 7;

// InputFeatures.

constexpr FeaturePair<CrossDeviceUserSegment::Feature>
    kCrossDeviceUserFeatures[] = {
        {CrossDeviceUserSegment::kFeatureDeviceCount,
         features::LatestOrDefaultValue("Sync.DeviceCount2",
                                        /*bucket_count=*/28,
                                        /*default_value=*/0)},
        {CrossDeviceUserSegment::kFeatureDeviceCountPhone,
         features::LatestOrDefaultValue("Sync.DeviceCount2.Phone",
                                        /*bucket_count=*/28,
                                        /*default_value=*/0)},
        {CrossDeviceUserSegment::kFeatureDeviceCountDesktop,
         features::LatestOrDefaultValue("Sync.DeviceCount2.Desktop",
                                        /*bucket_count=*/28,
                                        /*default_value=*/0)},
        {CrossDeviceUserSegment::kFeatureDeviceCountTablet,
         features::LatestOrDefaultValue("Sync.DeviceCount2.Tablet",
                                        /*bucket_count=*/28,
                                        /*default_value=*/0)}};

}  // namespace

// static
std::unique_ptr<Config> CrossDeviceUserSegment::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformCrossDeviceUser)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kCrossDeviceUserKey;
  config->segmentation_uma_name = kCrossDeviceUserUmaName;
  config->AddSegmentId(SegmentId::CROSS_DEVICE_USER_SEGMENT,
                       std::make_unique<CrossDeviceUserSegment>());
  config->auto_execute_and_cache = true;
  return config;
}

CrossDeviceUserSegment::CrossDeviceUserSegment()
    : DefaultModelProvider(kCrossDeviceUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
CrossDeviceUserSegment::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kCrossDeviceUserMinSignalCollectionLength,
      kCrossDeviceUserSignalStorageLength);

  // Set features.
  writer.AddFeatures<Feature>(kCrossDeviceUserFeatures);

  //  Set OutputConfig.
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{kLabelNoCrossDeviceUsage, "NoCrossDeviceUsage"},
                {kLabelCrossDeviceMobile, "CrossDeviceMobile"},
                {kLabelCrossDeviceDesktop, "CrossDeviceDesktop"},
                {kLabelCrossDeviceTablet, "CrossDeviceTablet"},
                {kLabelCrossDeviceMobileAndDesktop,
                 "CrossDeviceMobileAndDesktop"},
                {kLabelCrossDeviceMobileAndTablet,
                 "CrossDeviceMobileAndTablet"},
                {kLabelCrossDeviceDesktopAndTablet,
                 "CrossDeviceDesktopAndTablet"},
                {kLabelCrossDeviceAllDeviceTypes, "CrossDeviceAllDeviceTypes"},
                {kLabelCrossDeviceOther, "CrossDeviceOther"}},
      /*underflow_label=*/"NoCrossDeviceUsage");
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, kCrossDeviceUserSegmentSelectionTTLDays,
      /*time_unit=*/proto::TimeUnit::DAY);

  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

void CrossDeviceUserSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != Feature::kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  Label segment = kLabelNoCrossDeviceUsage;

  float phone_count = inputs[kFeatureDeviceCountPhone];
  float desktop_count = inputs[kFeatureDeviceCountDesktop];
  float tablet_count = inputs[kFeatureDeviceCountTablet];

// Check for current device type and subtract it from the device count
// calculation.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  desktop_count -= 1;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    tablet_count -= 1;
  } else {
    phone_count -= 1;
  }
#endif

  const bool multi_device_active = inputs[kFeatureDeviceCount] >= 2;
  const bool phone_active = phone_count >= 1;
  const bool desktop_active = desktop_count >= 1;
  const bool tablet_active = tablet_count >= 1;

  if (multi_device_active) {
    if (phone_active && desktop_active && tablet_active) {
      segment = kLabelCrossDeviceAllDeviceTypes;
    } else if (phone_active && desktop_active) {
      segment = kLabelCrossDeviceMobileAndDesktop;
    } else if (phone_active && tablet_active) {
      segment = kLabelCrossDeviceMobileAndTablet;
    } else if (desktop_active && tablet_active) {
      segment = kLabelCrossDeviceDesktopAndTablet;
    } else if (phone_active) {
      segment = kLabelCrossDeviceMobile;
    } else if (desktop_active) {
      segment = kLabelCrossDeviceDesktop;
    } else if (tablet_active) {
      segment = kLabelCrossDeviceTablet;
    } else {
      segment = kLabelCrossDeviceOther;
    }
  } else {
    segment = kLabelNoCrossDeviceUsage;
  }

  float result = segment;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
