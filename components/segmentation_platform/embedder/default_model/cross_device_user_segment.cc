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
enum class CrossDeviceUserSubsegment {
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

// Default parameters for Chrome Start model.
constexpr SegmentId kCrossDeviceUserSegmentId =
    SegmentId::CROSS_DEVICE_USER_SEGMENT;
constexpr int64_t kCrossDeviceUserSignalStorageLength = 28;
constexpr int64_t kCrossDeviceUserMinSignalCollectionLength = 1;
constexpr int kCrossDeviceUserSegmentSelectionTTLDays = 7;
constexpr int kCrossDeviceUserSegmentUnknownSelectionTTLDays = 7;

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

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string CrossDeviceUserSubsegmentToString(
    CrossDeviceUserSubsegment cross_device_group) {
  switch (cross_device_group) {
    case CrossDeviceUserSubsegment::kUnknown:
      return "Unknown";
    case CrossDeviceUserSubsegment::kNoCrossDeviceUsage:
      return "NoCrossDeviceUsage";
    case CrossDeviceUserSubsegment::kCrossDeviceMobile:
      return "CrossDeviceMobile";
    case CrossDeviceUserSubsegment::kCrossDeviceDesktop:
      return "CrossDeviceDesktop";
    case CrossDeviceUserSubsegment::kCrossDeviceTablet:
      return "CrossDeviceTablet";
    case CrossDeviceUserSubsegment::kCrossDeviceMobileAndDesktop:
      return "CrossDeviceMobileAndDesktop";
    case CrossDeviceUserSubsegment::kCrossDeviceMobileAndTablet:
      return "CrossDeviceMobileAndTablet";
    case CrossDeviceUserSubsegment::kCrossDeviceDesktopAndTablet:
      return "CrossDeviceDesktopAndTablet";
    case CrossDeviceUserSubsegment::kCrossDeviceAllDeviceTypes:
      return "CrossDeviceAllDeviceTypes";
    case CrossDeviceUserSubsegment::kCrossDeviceOther:
      return "CrossDeviceOther";
  }
}

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
  config->segment_selection_ttl =
      base::Days(kCrossDeviceUserSegmentSelectionTTLDays);
  config->unknown_selection_ttl =
      base::Days(kCrossDeviceUserSegmentUnknownSelectionTTLDays);
  config->is_boolean_segment = true;
  return config;
}

CrossDeviceUserSegment::CrossDeviceUserSegment()
    : DefaultModelProvider(kCrossDeviceUserSegmentId) {}

absl::optional<std::string> CrossDeviceUserSegment::GetSubsegmentName(
    int subsegment_rank) {
  DCHECK(RANK(CrossDeviceUserSubsegment::kUnknown) <= subsegment_rank &&
         subsegment_rank <= RANK(CrossDeviceUserSubsegment::kMaxValue));
  CrossDeviceUserSubsegment subgroup =
      static_cast<CrossDeviceUserSubsegment>(subsegment_rank);
  return CrossDeviceUserSubsegmentToString(subgroup);
}

std::unique_ptr<DefaultModelProvider::ModelConfig>
CrossDeviceUserSegment::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kCrossDeviceUserMinSignalCollectionLength,
      kCrossDeviceUserSignalStorageLength);

  writer.AddBooleanSegmentDiscreteMappingWithSubsegments(
      kCrossDeviceUserKey, 1, RANK(CrossDeviceUserSubsegment::kMaxValue));

  // Set features.
  writer.AddUmaFeatures(kCrossDeviceUserUMAFeatures.data(),
                        kCrossDeviceUserUMAFeatures.size());

  constexpr int kModelVersion = 1;
  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

void CrossDeviceUserSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kCrossDeviceUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }
  CrossDeviceUserSubsegment segment =
      CrossDeviceUserSubsegment::kNoCrossDeviceUsage;

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
      segment = CrossDeviceUserSubsegment::kCrossDeviceAllDeviceTypes;
    } else if (phone_active && desktop_active) {
      segment = CrossDeviceUserSubsegment::kCrossDeviceMobileAndDesktop;
    } else if (phone_active && tablet_active) {
      segment = CrossDeviceUserSubsegment::kCrossDeviceMobileAndTablet;
    } else if (desktop_active && tablet_active) {
      segment = CrossDeviceUserSubsegment::kCrossDeviceDesktopAndTablet;
    } else if (phone_active) {
      segment = CrossDeviceUserSubsegment::kCrossDeviceMobile;
    } else if (desktop_active) {
      segment = CrossDeviceUserSubsegment::kCrossDeviceDesktop;
    } else if (tablet_active) {
      segment = CrossDeviceUserSubsegment::kCrossDeviceTablet;
    } else {
      segment = CrossDeviceUserSubsegment::kCrossDeviceOther;
    }
  } else {
    segment = segment = CrossDeviceUserSubsegment::kNoCrossDeviceUsage;
  }

  float result = RANK(segment);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
