// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"

#include <array>

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {

// List of sub-segments for Power segment.
enum class PowerUserSubsegment {
  kUnknown = 0,

  kNone = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,

  kMaxValue = kHigh
};

#define RANK(x) static_cast<int>(x)

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kPowerUserSegmentId = SegmentId::POWER_USER_SEGMENT;
constexpr int64_t kPowerUserSignalStorageLength = 28;
constexpr int64_t kPowerUserMinSignalCollectionLength = 7;

// InputFeatures.

constexpr std::array<int32_t, 2> kEnumHistorgram0And1{0, 1};
constexpr std::array<int32_t, 1> kEnumHistorgram1{1};
constexpr std::array<int32_t, 1> kEnumHistorgram0{0};

constexpr std::array<MetadataWriter::UMAFeature, 27> kPowerUserUMAFeatures = {
    // 0
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Download.Start.PerProfileType",
        28,
        kEnumHistorgram0.data(),
        kEnumHistorgram0.size()),
    // 1
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuDownloadManager", 28),

    // 2
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuDownloadPage", 28),
    // 3
    MetadataWriter::UMAFeature::FromUserAction("MobileTabSwitched", 28),
    // 4
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRequestDesktopSite",
                                               28),
    // 5
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuHistory", 28),
    // 6
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuSettings", 28),
    // 7
    MetadataWriter::UMAFeature::FromUserAction(
        "SharingHubAndroid.SendTabToSelfSelected",
        28),

    // 8
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuShare", 28),
    // 9
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuAddToBookmarks", 28),
    // 10
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuAllBookmarks", 28),
    // 11
    MetadataWriter::UMAFeature::FromUserAction("MobileOmniboxVoiceSearch", 28),
    // 12
    MetadataWriter::UMAFeature::FromUserAction("Media.Controls.Cast", 28),
    // 13
    MetadataWriter::UMAFeature::FromUserAction("Media.Controls.CastOverlay",
                                               28),
    // 14
    MetadataWriter::UMAFeature::FromUserAction("IncognitoMode_Started", 28),
    // 15
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Autofill.KeyMetrics.FillingAcceptance.Address",
        28,
        kEnumHistorgram0And1.data(),
        kEnumHistorgram0And1.size()),
    // 16
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Autofill.KeyMetrics.FillingAcceptance.CreditCard",
        28,
        kEnumHistorgram0And1.data(),
        kEnumHistorgram0And1.size()),
    // 17
    MetadataWriter::UMAFeature::FromValueHistogram("Media.OutputStreamDuration",
                                                   28,
                                                   proto::Aggregation::SUM),
    // 18
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "PasswordManager.FillingSource",
        28,
        kEnumHistorgram1.data(),
        kEnumHistorgram1.size()),
    // 19
    MetadataWriter::UMAFeature::FromValueHistogram("Media.InputStreamDuration",
                                                   28,
                                                   proto::Aggregation::SUM),

    // 20
    MetadataWriter::UMAFeature::FromEnumHistogram("UMA.ProfileSignInStatusV2",
                                                  28,
                                                  kEnumHistorgram0.data(),
                                                  kEnumHistorgram0.size()),
    // 21
    MetadataWriter::UMAFeature::FromEnumHistogram("UMA.ProfileSyncStatusV2",
                                                  28,
                                                  kEnumHistorgram0.data(),
                                                  kEnumHistorgram0.size()),
    // 22
    MetadataWriter::UMAFeature::FromValueHistogram(
        "Android.PhotoPicker.DialogAction",
        28,
        proto::Aggregation::COUNT),
    // 23
    MetadataWriter::UMAFeature::FromValueHistogram(
        "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular",
        28,
        proto::Aggregation::SUM),
    // 24
    MetadataWriter::UMAFeature::FromValueHistogram(
        "DataUse.TrafficSize.User.Upstream.Foreground.Cellular",
        28,
        proto::Aggregation::SUM),
    // 25
    MetadataWriter::UMAFeature::FromUserAction("TabGroup.Created.OpenInNewTab",
                                               28),
    // 26
    MetadataWriter::UMAFeature::FromValueHistogram("Session.TotalDuration",
                                                   28,
                                                   proto::Aggregation::SUM),
};

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string PowerUserSubsegmentToString(PowerUserSubsegment power_group) {
  switch (power_group) {
    case PowerUserSubsegment::kUnknown:
      return "Unknown";
    case PowerUserSubsegment::kNone:
      return "None";
    case PowerUserSubsegment::kLow:
      return "Low";
    case PowerUserSubsegment::kMedium:
      return "Medium";
    case PowerUserSubsegment::kHigh:
      return "High";
  }
}

}  // namespace

// static
std::unique_ptr<Config> PowerUserSegment::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformPowerUserFeature)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kPowerUserKey;
  config->segmentation_uma_name = kPowerUserUmaName;
  config->AddSegmentId(SegmentId::POWER_USER_SEGMENT,
                       std::make_unique<PowerUserSegment>());
  config->segment_selection_ttl = base::Days(7);
  config->unknown_selection_ttl = base::Days(7);
  config->is_boolean_segment = true;

  return config;
}

PowerUserSegment::PowerUserSegment() : ModelProvider(kPowerUserSegmentId) {}

absl::optional<std::string> PowerUserSegment::GetSubsegmentName(
    int subsegment_rank) {
  DCHECK(RANK(PowerUserSubsegment::kUnknown) <= subsegment_rank &&
         subsegment_rank <= RANK(PowerUserSubsegment::kMaxValue));
  PowerUserSubsegment subgroup =
      static_cast<PowerUserSubsegment>(subsegment_rank);
  return PowerUserSubsegmentToString(subgroup);
}

void PowerUserSegment::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kPowerUserMinSignalCollectionLength, kPowerUserSignalStorageLength);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMappingWithSubsegments(
      kPowerUserKey, RANK(PowerUserSubsegment::kMedium),
      RANK(PowerUserSubsegment::kMaxValue));

  // Set features.
  writer.AddUmaFeatures(kPowerUserUMAFeatures.data(),
                        kPowerUserUMAFeatures.size());

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kPowerUserSegmentId,
                          std::move(chrome_start_metadata), kModelVersion));
}

static void AddToScoreIf(bool usage, int& score) {
  if (usage)
    score++;
}

void PowerUserSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kPowerUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  PowerUserSubsegment segment = PowerUserSubsegment::kNone;

  int score = 0;

  AddToScoreIf(/*download_usage */
               (inputs[0] + inputs[1] + inputs[2]) >= 2, score);
  AddToScoreIf(/*tabs_usage */ inputs[3] >= 4, score);
  AddToScoreIf(/*desktop_site_usage */ inputs[4] >= 2, score);
  AddToScoreIf(/*history_usage */ inputs[5] >= 2, score);
  AddToScoreIf(/*settings_usage */ inputs[6] >= 2, score);
  AddToScoreIf(/*share_usage */ (inputs[7] + inputs[8]) >= 2, score);
  AddToScoreIf(/*bookmark_usage */ (inputs[9] + inputs[10]) >= 2, score);
  AddToScoreIf(/*voice_usage */ inputs[11] >= 2, score);
  AddToScoreIf(/*cast_usage */ (inputs[12] + inputs[13]) >= 2, score);
  AddToScoreIf(/*incognito_usage */ inputs[14] >= 2, score);
  AddToScoreIf(/*autofill_usage */ (inputs[15] + inputs[16]) >= 2, score);
  AddToScoreIf(/*media_watch_usage */ inputs[17] > 30 * 1000,
               score);  // 30 seconds.
  AddToScoreIf(/*password_usage */ inputs[18] >= 2, score);
  AddToScoreIf(/*audio_usage */ inputs[19] >= 5000, score);  // 5 seconds.
  AddToScoreIf(/*signin_and_sync */ inputs[20] > 0 && inputs[21] > 0, score);
  AddToScoreIf(/*media_upload_usage */ inputs[22] >= 2, score);
  AddToScoreIf(/*data_upload_usage */
               (inputs[23] + inputs[24]) > 10000, score);  // 10Kb upload.
  AddToScoreIf(/*tab_group_usage */ inputs[25] >= 2, score);
  AddToScoreIf(/*active_usage */ inputs[26] >= 15 * 60 * 1000,
               score);  // 15 minutes

  // Max score is 19.
  if (score >= 10) {
    segment = PowerUserSubsegment::kHigh;
  } else if (score >= 7) {
    segment = PowerUserSubsegment::kMedium;
  } else if (score >= 3) {
    segment = PowerUserSubsegment::kLow;
  } else {
    segment = PowerUserSubsegment::kNone;
  }

  float result = RANK(segment);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

bool PowerUserSegment::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
