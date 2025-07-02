// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/ios_default_browser_promo.h"

#include <memory>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for IosDefaultBrowserPromo model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_IOS_DEFAULT_BROWSER_PROMO;
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every time.
constexpr int64_t kResultTTLMinutes = 1;

constexpr std::array<const char*, 1> kOutputLabels = {
    kIosDefaultBrowserPromoShowLabel};

// Input features:
constexpr std::array<int32_t, 1> kEnumValueForFirstRunStageSignIn{
    /*kWelcomeAndSigninScreenCompletionWithSignIn=*/13};
constexpr std::array<int32_t, 1> kEnumValueForFirstRunStageOpenSettings{
    /*kDefaultBrowserScreenCompletionWithSettings=*/10};
constexpr std::array<int32_t, 1> kEnumValueForPageLoadCountsPageLoadNavigation{
    /*PageLoadCountNavigationType::PageLoadNavigation=*/2};
constexpr std::array<int32_t, 1> kEnumValueForLaunchSourceAppIcon{
    /*AppIcon=*/0};
constexpr std::array<int32_t, 1> kEnumValueForLaunchSourceDefaultIntent{
    /*DefaultIntent=*/4};
constexpr std::array<int32_t, 1> kEnumValueForLaunchSourceLinkOpen{
    /*LinkOpen=*/5};
constexpr std::array<int32_t, 1> kEnumValueForNTPImpressionFeedVisible{
    /*FeedVisible=*/1};
constexpr std::array<int32_t, 1> kEnumValueForIncognitoInterstitialEnabled{
    /*Enabled=*/1};
constexpr std::array<int32_t, 2> kEnumValueForOmniboxSearchVsURL{/*URL=*/0,
                                                                 /*search=*/1};
constexpr std::array<float, 1> kDefaultBrowserFeatureDefaultValue{0};

constexpr std::array<MetadataWriter::UMAFeature, 32> kUMAFeatures = {
    MetadataWriter::UMAFeature::FromValueHistogram(
        "FirstRun.Stage",
        28,
        proto::Aggregation::COUNT_BOOLEAN,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "Session.TotalDuration",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "Session.TotalDuration",
        28,
        proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_ENUM,
        .name = "FirstRun.Stage",
        .bucket_count = 28,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT_BOOLEAN,
        .enum_ids_size = kEnumValueForFirstRunStageSignIn.size(),
        .accepted_enum_ids = kEnumValueForFirstRunStageSignIn.data()},

    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_ENUM,
        .name = "FirstRun.Stage",
        .bucket_count = 28,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT_BOOLEAN,
        .enum_ids_size = kEnumValueForFirstRunStageOpenSettings.size(),
        .accepted_enum_ids = kEnumValueForFirstRunStageOpenSettings.data()},

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.PageLoadCount.Counts",
        28,
        kEnumValueForPageLoadCountsPageLoadNavigation.data(),
        kEnumValueForPageLoadCountsPageLoadNavigation.size()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "IOS.LaunchSource",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.LaunchSource",
        28,
        kEnumValueForLaunchSourceAppIcon.data(),
        kEnumValueForLaunchSourceAppIcon.size()),

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.LaunchSource",
        28,
        kEnumValueForLaunchSourceDefaultIntent.data(),
        kEnumValueForLaunchSourceDefaultIntent.size()),

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.LaunchSource",
        28,
        kEnumValueForLaunchSourceLinkOpen.data(),
        kEnumValueForLaunchSourceLinkOpen.size()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "Startup.MobileSessionStartFromApps",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabOpened", 28),

    MetadataWriter::UMAFeature::FromUserAction("MobileTabGridEntered", 28),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "IOS.Start.Impression",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "IOS.NTP.Impression",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.NTP.Impression",
        28,
        kEnumValueForNTPImpressionFeedVisible.data(),
        kEnumValueForNTPImpressionFeedVisible.size()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "IOS.Incognito.TimeSpent",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.IncognitoInterstitial.Settings",
        28,
        kEnumValueForIncognitoInterstitialEnabled.data(),
        kEnumValueForIncognitoInterstitialEnabled.size()),

    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Omnibox.SuggestionUsed.SearchVsUrl",
        28,
        kEnumValueForOmniboxSearchVsURL.data(),
        kEnumValueForOmniboxSearchVsURL.size()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "NewTabPage.TimeSpent",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "PasswordManager.ProfileStore.TotalAccountsHiRes3.ByType.Overall."
        "WithoutCustomPassphrase",
        28,
        proto::Aggregation::LATEST_OR_DEFAULT,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "PasswordManager.AccountStore.TotalAccountsHiRes3.ByType.Overall."
        "WithoutCustomPassphrase",
        28,
        proto::Aggregation::LATEST_OR_DEFAULT,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "IOS.CredentialExtension.IsEnabled.Startup",
        28,
        proto::Aggregation::LATEST_OR_DEFAULT,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "PasswordManager.BulkCheck.UserAction",
        28,
        proto::Aggregation::SUM,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "Session.TotalDuration.WithAccount",
        28,
        proto::Aggregation::SUM_BOOLEAN,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromValueHistogram(
        "Signin.IOSNumberOfDeviceAccounts",
        28,
        proto::Aggregation::SUM_BOOLEAN,
        kDefaultBrowserFeatureDefaultValue.size(),
        kDefaultBrowserFeatureDefaultValue.data()),

    MetadataWriter::UMAFeature::FromUserAction("Bookmarks.FolderAdded", 28),

    MetadataWriter::UMAFeature::FromUserAction(
        "IOSMagicStackSafetyCheckFreshSignal",
        28),

    MetadataWriter::UMAFeature::FromUserAction("Forward", 28),

    MetadataWriter::UMAFeature::FromUserAction("Back", 28),

    MetadataWriter::UMAFeature::FromUserAction("MobileStackSwipeCancelled", 28),

    MetadataWriter::UMAFeature::FromUserAction("MobileToolbarForward", 28),
};

// TODO(crbug.com/407788921): Clean up the killswitch.
BASE_FEATURE(kIOSDefaultBrowserPromoDefaultModel,
             "IOSDefaultBrowserPromoDefaultModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// static
std::unique_ptr<Config> IosDefaultBrowserPromo::GetConfig() {
  if (!base::FeatureList::IsEnabled(kIOSDefaultBrowserPromoDefaultModel)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kIosDefaultBrowserPromoKey;
  config->segmentation_uma_name = kIosDefaultBrowserPromoUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<IosDefaultBrowserPromo>());
  config->auto_execute_and_cache = false;
  return config;
}

IosDefaultBrowserPromo::IosDefaultBrowserPromo()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
IosDefaultBrowserPromo::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  metadata.set_upload_tensors(false);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kOutputLabels,
                                                kOutputLabels.size(),
                                                /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void IosDefaultBrowserPromo::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Fail execution, not expecting to execute model.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

}  // namespace segmentation_platform
