// Copyright 2022 The Chromium Authors
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

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kPowerUserSegmentId = SegmentId::POWER_USER_SEGMENT;
constexpr int64_t kPowerUserSignalStorageLength = 28;
constexpr int64_t kPowerUserMinSignalCollectionLength = 7;

// InputFeatures.

constexpr std::array<int32_t, 2> kEnumHistorgram0And1{0, 1};
constexpr std::array<int32_t, 1> kEnumHistorgram1{1};
constexpr std::array<int32_t, 1> kEnumHistorgram0{0};

constexpr FeaturePair<PowerUserSegment::Feature> kFeatures[] = {
    {PowerUserSegment::kFeatureDownloadStartPerProfileType,
     features::UMAEnum("Download.Start.PerProfileType", 28, kEnumHistorgram0)},
    {PowerUserSegment::kFeatureMobileMenuDownloadManager,
     features::UserAction("MobileMenuDownloadManager", 28)},
    {PowerUserSegment::kFeatureMobileMenuDownloadPage,
     features::UserAction("MobileMenuDownloadPage", 28)},
    {PowerUserSegment::kFeatureMobileTabSwitched,
     features::UserAction("MobileTabSwitched", 28)},
    {PowerUserSegment::kFeatureMobileMenuRequestDesktopSite,
     features::UserAction("MobileMenuRequestDesktopSite", 28)},
    {PowerUserSegment::kFeatureMobileMenuHistory,
     features::UserAction("MobileMenuHistory", 28)},
    {PowerUserSegment::kFeatureMobileMenuSettings,
     features::UserAction("MobileMenuSettings", 28)},
    {PowerUserSegment::kFeatureSharingHubAndroidSendTabToSelfSelected,
     features::UserAction("SharingHubAndroid.SendTabToSelfSelected", 28)},
    {PowerUserSegment::kFeatureMobileMenuShare,
     features::UserAction("MobileMenuShare", 28)},
    {PowerUserSegment::kFeatureMobileMenuAddToBookmarks,
     features::UserAction("MobileMenuAddToBookmarks", 28)},
    {PowerUserSegment::kFeatureMobileMenuAllBookmarks,
     features::UserAction("MobileMenuAllBookmarks", 28)},
    {PowerUserSegment::kFeatureMobileOmniboxVoiceSearch,
     features::UserAction("MobileOmniboxVoiceSearch", 28)},
    {PowerUserSegment::kFeatureMediaControlsCast,
     features::UserAction("Media.Controls.Cast", 28)},
    {PowerUserSegment::kFeatureMediaControlsCastOverlay,
     features::UserAction("Media.Controls.CastOverlay", 28)},
    {PowerUserSegment::kFeatureIncognitoModeStarted,
     features::UserAction("IncognitoMode_Started", 28)},
    {PowerUserSegment::kFeatureAutofillKeyMetricsFillingAcceptanceAddress,
     features::UMAEnum("Autofill.KeyMetrics.FillingAcceptance.Address",
                       28,
                       kEnumHistorgram0And1)},
    {PowerUserSegment::kFeatureAutofillKeyMetricsFillingAcceptanceCreditCard,
     features::UMAEnum("Autofill.KeyMetrics.FillingAcceptance.CreditCard",
                       28,
                       kEnumHistorgram0And1)},
    {PowerUserSegment::kFeatureMediaOutputStreamDuration,
     features::UMASum("Media.OutputStreamDuration", 28)},
    {PowerUserSegment::kFeaturePasswordManagerFillingSource,
     features::UMAEnum("PasswordManager.FillingSource", 28, kEnumHistorgram1)},
    {PowerUserSegment::kFeatureMediaInputStreamDuration,
     features::UMASum("Media.InputStreamDuration", 28)},
    {PowerUserSegment::kFeatureUMAProfileSignInStatusV2,
     features::UMAEnum("UMA.ProfileSignInStatusV2", 28, kEnumHistorgram0)},
    {PowerUserSegment::kFeatureUMAProfileSyncStatusV2,
     features::UMAEnum("UMA.ProfileSyncStatusV2", 28, kEnumHistorgram0)},
    {PowerUserSegment::kFeatureAndroidPhotoPickerDiaglogAction,
     features::UMACount("Android.PhotoPicker.DialogAction", 28)},
    {PowerUserSegment::
         kFeatureDataUseTrafficSizeUserUpstreamForegroundNotCellular,
     features::UMASum(
         "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular",
         28)},
    {PowerUserSegment::kFeatureDataUseTrafficSizeUserUpstreamForegroundCellular,
     features::UMASum("DataUse.TrafficSize.User.Upstream.Foreground.Cellular",
                      28)},
    {PowerUserSegment::kFeatureTabGroupCreatedOpenInNewTab,
     features::UserAction("TabGroup.Created.OpenInNewTab", 28)},
    {PowerUserSegment::kFeatureSessionTotalDuration,
     features::UMASum("Session.TotalDuration", 28)},
};

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string PowerUserBinToString(PowerUserSegment::Label label) {
  switch (label) {
    case PowerUserSegment::kLabelUnknown:
      return "Unknown";
    case PowerUserSegment::kLabelNone:
      return "None";
    case PowerUserSegment::kLabelLow:
      return "Low";
    case PowerUserSegment::kLabelMedium:
      return "Medium";
    case PowerUserSegment::kLabelHigh:
      return "High";
    case PowerUserSegment::kLabelCount:
      NOTREACHED();
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
  config->auto_execute_and_cache = true;
  return config;
}

PowerUserSegment::PowerUserSegment()
    : DefaultModelProvider(kPowerUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
PowerUserSegment::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kPowerUserMinSignalCollectionLength, kPowerUserSignalStorageLength);

  writer.AddOutputConfigForBinnedClassifier(
      {
          {kLabelNone, PowerUserBinToString(kLabelNone)},
          {kLabelLow, PowerUserBinToString(kLabelLow)},
          {kLabelMedium, PowerUserBinToString(kLabelMedium)},
          {kLabelHigh, PowerUserBinToString(kLabelHigh)},
      },
      "Unknown");
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/7,
      /*time_unit=*/proto::TimeUnit::DAY);

  // Set features.
  writer.AddFeatures<Feature>(kFeatures);

  constexpr int kModelVersion = 1;
  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

static void AddToScoreIf(bool usage, int& score) {
  if (usage)
    score++;
}

void PowerUserSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  Label segment = kLabelNone;

  int score = 0;

  AddToScoreIf((inputs[kFeatureDownloadStartPerProfileType] +
                inputs[kFeatureMobileMenuDownloadManager] +
                inputs[kFeatureMobileMenuDownloadPage]) >= 2,
               score);
  AddToScoreIf(inputs[kFeatureMobileTabSwitched] >= 4, score);
  AddToScoreIf(inputs[kFeatureMobileMenuRequestDesktopSite] >= 2, score);
  AddToScoreIf(inputs[kFeatureMobileMenuHistory] >= 2, score);
  AddToScoreIf(inputs[kFeatureMobileMenuSettings] >= 2, score);
  AddToScoreIf((inputs[kFeatureSharingHubAndroidSendTabToSelfSelected] +
                inputs[kFeatureMobileMenuShare]) >= 2,
               score);
  AddToScoreIf((inputs[kFeatureMobileMenuAddToBookmarks] +
                inputs[kFeatureMobileMenuAllBookmarks]) >= 2,
               score);
  AddToScoreIf(inputs[kFeatureMobileOmniboxVoiceSearch] >= 2, score);
  AddToScoreIf((inputs[kFeatureMediaControlsCast] +
                inputs[kFeatureMediaControlsCastOverlay]) >= 2,
               score);
  AddToScoreIf(inputs[kFeatureIncognitoModeStarted] >= 2, score);
  AddToScoreIf(
      (inputs[kFeatureAutofillKeyMetricsFillingAcceptanceAddress] +
       inputs[kFeatureAutofillKeyMetricsFillingAcceptanceCreditCard]) >= 2,
      score);
  AddToScoreIf(inputs[kFeatureMediaOutputStreamDuration] > 30 * 1000,
               score);  // 30 seconds.
  AddToScoreIf(inputs[kFeaturePasswordManagerFillingSource] >= 2, score);
  AddToScoreIf(inputs[kFeatureMediaInputStreamDuration] >= 5000,
               score);  // 5 seconds.
  AddToScoreIf(inputs[kFeatureUMAProfileSignInStatusV2] > 0 &&
                   inputs[kFeatureUMAProfileSyncStatusV2] > 0,
               score);
  AddToScoreIf(inputs[kFeatureAndroidPhotoPickerDiaglogAction] >= 2, score);
  AddToScoreIf(
      (inputs[kFeatureDataUseTrafficSizeUserUpstreamForegroundNotCellular] +
       inputs[kFeatureDataUseTrafficSizeUserUpstreamForegroundCellular]) >
          10000,
      score);  // 10Kb upload.
  AddToScoreIf(inputs[kFeatureTabGroupCreatedOpenInNewTab] >= 2, score);
  AddToScoreIf(inputs[kFeatureSessionTotalDuration] >= 15 * 60 * 1000,
               score);  // 15 minutes

  // Max score is 19.
  if (score >= 10) {
    segment = kLabelHigh;
  } else if (score >= 7) {
    segment = kLabelMedium;
  } else if (score >= 3) {
    segment = kLabelLow;
  } else {
    segment = kLabelNone;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, segment)));
}

#undef RANK
}  // namespace segmentation_platform
