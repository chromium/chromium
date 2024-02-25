// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"

#include <array>

#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "ui/base/device_form_factor.h"

namespace segmentation_platform {

namespace {

// List of sub-segments for cross device segment.
enum class CrossDeviceUserBin {
  kUnknown = 0,
  kNoCrossDeviceUsage = 1,
  kCrossDeviceMobile = 2,
  kCrossDeviceDesktop = 3,
  kCrossDeviceTablet = 4,
  kCrossDeviceMobileAndDesktop = 5,
  kCrossDeviceMobileAndTablet = 6,
  kCrossDeviceDesktopAndTablet = 7,
  kCrossDeviceAllDeviceTypes = 8,
  kCrossDeviceOther = 9,
  kMaxValue = kCrossDeviceOther
};

#define RANK(x) static_cast<int>(x)

using proto::SegmentId;

// Default parameters for cross device model.
constexpr int kModelVersion = 2;
constexpr SegmentId kCrossDeviceUserSegmentId =
    SegmentId::CROSS_DEVICE_USER_SEGMENT;
constexpr int64_t kCrossDeviceUserSignalStorageLength = 28;
constexpr int64_t kCrossDeviceUserMinSignalCollectionLength = 1;
constexpr int kCrossDeviceUserSegmentSelectionTTLDays = 7;

// InputFeatures.

constexpr std::array<float, 1> kCrossDeviceFeatureDefaultValue{0};

constexpr std::array<MetadataWriter::UMAFeature, 4>
    kCrossDeviceUserUMAFeatures = {
        MetadataWriter::UMAFeature::FromValueHistogram(
            "Sync.DeviceCount2",
            28,
            proto::Aggregation::LATEST_OR_DEFAULT,
            kCrossDeviceFeatureDefaultValue.size(),
            kCrossDeviceFeatureDefaultValue.data()),
        MetadataWriter::UMAFeature::FromValueHistogram(
            "Sync.DeviceCount2.Phone",
            28,
            proto::Aggregation::LATEST_OR_DEFAULT,
            kCrossDeviceFeatureDefaultValue.size(),
            kCrossDeviceFeatureDefaultValue.data()),
        MetadataWriter::UMAFeature::FromValueHistogram(
            "Sync.DeviceCount2.Desktop",
            28,
            proto::Aggregation::LATEST_OR_DEFAULT,
            kCrossDeviceFeatureDefaultValue.size(),
            kCrossDeviceFeatureDefaultValue.data()),
        MetadataWriter::UMAFeature::FromValueHistogram(
            "Sync.DeviceCount2.Tablet",
            28,
            proto::Aggregation::LATEST_OR_DEFAULT,
            kCrossDeviceFeatureDefaultValue.size(),
            kCrossDeviceFeatureDefaultValue.data())};

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
  writer.AddUmaFeatures(kCrossDeviceUserUMAFeatures.data(),
                        kCrossDeviceUserUMAFeatures.size());

  //  Set OutputConfig.
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{1, kNoCrossDeviceUsage},
                {2, kCrossDeviceMobile},
                {3, kCrossDeviceDesktop},
                {4, kCrossDeviceTablet},
                {5, kCrossDeviceMobileAndDesktop},
                {6, kCrossDeviceMobileAndTablet},
                {7, kCrossDeviceDesktopAndTablet},
                {8, kCrossDeviceAllDeviceTypes},
                {9, kCrossDeviceOther}},
      /*underflow_label=*/kNoCrossDeviceUsage);
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
  if (inputs.size() != kCrossDeviceUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  CrossDeviceUserBin segment = CrossDeviceUserBin::kNoCrossDeviceUsage;

  float phone_count = inputs[1];
  float desktop_count = inputs[2];
  float tablet_count = inputs[3];

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

  const bool multi_device_active = inputs[0] >= 2;
  const bool phone_active = phone_count >= 1;
  const bool desktop_active = desktop_count >= 1;
  const bool tablet_active = tablet_count >= 1;

  if (multi_device_active) {
    if (phone_active && desktop_active && tablet_active) {
      segment = CrossDeviceUserBin::kCrossDeviceAllDeviceTypes;
    } else if (phone_active && desktop_active) {
      segment = CrossDeviceUserBin::kCrossDeviceMobileAndDesktop;
    } else if (phone_active && tablet_active) {
      segment = CrossDeviceUserBin::kCrossDeviceMobileAndTablet;
    } else if (desktop_active && tablet_active) {
      segment = CrossDeviceUserBin::kCrossDeviceDesktopAndTablet;
    } else if (phone_active) {
      segment = CrossDeviceUserBin::kCrossDeviceMobile;
    } else if (desktop_active) {
      segment = CrossDeviceUserBin::kCrossDeviceDesktop;
    } else if (tablet_active) {
      segment = CrossDeviceUserBin::kCrossDeviceTablet;
    } else {
      segment = CrossDeviceUserBin::kCrossDeviceOther;
    }
  } else {
    segment = segment = CrossDeviceUserBin::kNoCrossDeviceUsage;
  }

  float result = RANK(segment);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
