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

constexpr LabelPair<IosDefaultBrowserPromo::Label> kLabels[] = {
    {IosDefaultBrowserPromo::kLabelShow, kIosDefaultBrowserPromoShowLabel}};

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

constexpr FeaturePair<IosDefaultBrowserPromo::Feature> kFeatures[] = {
    {IosDefaultBrowserPromo::kFeatureFirstRunStageCount,
     features::UMAAggregate("FirstRun.Stage",
                            28,
                            proto::Aggregation::COUNT_BOOLEAN)},
    {IosDefaultBrowserPromo::kFeatureSessionTotalDurationSum,
     features::UMASum("Session.TotalDuration", 28)},
    {IosDefaultBrowserPromo::kFeatureSessionTotalDurationBooleanTrueCount,
     features::UMAAggregate(
         "Session.TotalDuration",
         28,
         proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT)},
    {IosDefaultBrowserPromo::kFeatureFirstRunStageSignInCount,
     features::UMAEnum("FirstRun.Stage",
                       28,
                       base::span(kEnumValueForFirstRunStageSignIn))},
    {IosDefaultBrowserPromo::kFeatureFirstRunStageOpenSettingsCount,
     features::UMAEnum("FirstRun.Stage",
                       28,
                       base::span(kEnumValueForFirstRunStageOpenSettings))},
    {IosDefaultBrowserPromo::kFeaturePageLoadCountsPageLoadNavigationCount,
     features::UMAEnum(
         "IOS.PageLoadCount.Counts",
         28,
         base::span(kEnumValueForPageLoadCountsPageLoadNavigation))},
    {IosDefaultBrowserPromo::kFeatureLaunchSourceSum,
     features::UMASum("IOS.LaunchSource", 28)},
    {IosDefaultBrowserPromo::kFeatureLaunchSourceAppIconCount,
     features::UMAEnum("IOS.LaunchSource",
                       28,
                       base::span(kEnumValueForLaunchSourceAppIcon))},
    {IosDefaultBrowserPromo::kFeatureLaunchSourceDefaultIntentCount,
     features::UMAEnum("IOS.LaunchSource",
                       28,
                       base::span(kEnumValueForLaunchSourceDefaultIntent))},
    {IosDefaultBrowserPromo::kFeatureLaunchSourceLinkOpenCount,
     features::UMAEnum("IOS.LaunchSource",
                       28,
                       base::span(kEnumValueForLaunchSourceLinkOpen))},
    {IosDefaultBrowserPromo::kFeatureMobileSessionStartFromAppsSum,
     features::UMASum("Startup.MobileSessionStartFromApps", 28)},
    {IosDefaultBrowserPromo::kFeatureMobileNewTabOpenedCount,
     features::UserAction("MobileNewTabOpened", 28)},
    {IosDefaultBrowserPromo::kFeatureMobileTabGridEnteredCount,
     features::UserAction("MobileTabGridEntered", 28)},
    {IosDefaultBrowserPromo::kFeatureStartImpressionSum,
     features::UMASum("IOS.Start.Impression", 28)},
    {IosDefaultBrowserPromo::kFeatureNTPImpressionSum,
     features::UMASum("IOS.NTP.Impression", 28)},
    {IosDefaultBrowserPromo::kFeatureNTPImpressionFeedVisibleCount,
     features::UMAEnum("IOS.NTP.Impression",
                       28,
                       base::span(kEnumValueForNTPImpressionFeedVisible))},
    {IosDefaultBrowserPromo::kFeatureIncognitoTimeSpentSum,
     features::UMASum("IOS.Incognito.TimeSpent", 28)},
    {IosDefaultBrowserPromo::kFeatureIncognitoInterstitialEnabledCount,
     features::UMAEnum("IOS.IncognitoInterstitial.Settings",
                       28,
                       base::span(kEnumValueForIncognitoInterstitialEnabled))},
    {IosDefaultBrowserPromo::kFeatureOmniboxSearchVsUrlCount,
     features::UMAEnum("Omnibox.SuggestionUsed.SearchVsUrl",
                       28,
                       base::span(kEnumValueForOmniboxSearchVsURL))},
    {IosDefaultBrowserPromo::kFeatureNewTabPageTimeSpentSum,
     features::UMASum("NewTabPage.TimeSpent", 28)},
    {IosDefaultBrowserPromo::kFeatureProfileStorePasswordCount,
     features::LatestOrDefaultValue(
         "PasswordManager.ProfileStore.TotalAccountsHiRes3.ByType.Overall."
         "WithoutCustomPassphrase",
         28,
         0)},
    {IosDefaultBrowserPromo::kFeatureAccountStorePasswordCount,
     features::LatestOrDefaultValue(
         "PasswordManager.AccountStore.TotalAccountsHiRes3.ByType.Overall."
         "WithoutCustomPassphrase",
         28,
         0)},
    {IosDefaultBrowserPromo::kFeatureIOSCredentialExtensionEnabled,
     features::LatestOrDefaultValue("IOS.CredentialExtension.IsEnabled.Startup",
                                    28,
                                    0)},
    {IosDefaultBrowserPromo::kFeaturePasswordManagerBulkCheckSum,
     features::UMASum("PasswordManager.BulkCheck.UserAction", 28)},
    {IosDefaultBrowserPromo::kFeatureSessionTotalDurationWithAccountBooleanSum,
     features::UMAAggregate("Session.TotalDuration.WithAccount",
                            28,
                            proto::Aggregation::SUM_BOOLEAN)},
    {IosDefaultBrowserPromo::kFeatureSigninIOSNumberOfDeviceAccountsBooleanSum,
     features::UMAAggregate("Signin.IOSNumberOfDeviceAccounts",
                            28,
                            proto::Aggregation::SUM_BOOLEAN)},
    {IosDefaultBrowserPromo::kFeatureBookmarksFolderAddedCount,
     features::UserAction("Bookmarks.FolderAdded", 28)},
    {IosDefaultBrowserPromo::kFeatureIOSMagicStackSafetyCheckFreshSignalCount,
     features::UserAction("IOSMagicStackSafetyCheckFreshSignal", 28)},
    {IosDefaultBrowserPromo::kFeatureForwardCount,
     features::UserAction("Forward", 28)},
    {IosDefaultBrowserPromo::kFeatureBackCount,
     features::UserAction("Back", 28)},
    {IosDefaultBrowserPromo::kFeatureMobileStackSwipeCancelledCount,
     features::UserAction("MobileStackSwipeCancelled", 28)},
    {IosDefaultBrowserPromo::kFeatureMobileToolbarForwardCount,
     features::UserAction("MobileToolbarForward", 28)},
    {IosDefaultBrowserPromo::kFeatureClientAgeWeeks,
     features::InputContext(kClientAgeWeeks)},
    {IosDefaultBrowserPromo::kFeatureIsPhone, features::InputContext(kIsPhone)},
    {IosDefaultBrowserPromo::kFeatureIsCountryBRIIM,
     features::InputContext(kCountryBRIIM)},
    {IosDefaultBrowserPromo::kFeatureIsSegmentationAndroidPhone,
     features::InputContext(kSegmentationAndroidPhone)},
    {IosDefaultBrowserPromo::kFeatureIsSegmentationIOSPhoneChrome,
     features::InputContext(kSegmentationIOSPhoneChrome)},
    {IosDefaultBrowserPromo::kFeatureIsSegmentationSyncedFirstDevice,
     features::InputContext(kSegmentationSyncedAndFirstDevice)},
};

// TODO(crbug.com/407788921): Clean up the killswitch.
BASE_FEATURE(kIOSDefaultBrowserPromoDefaultModel,
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
  writer.AddOutputConfigForMultiClassClassifier<Label>(kLabels,
                                                       /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  // Set features.
  writer.AddFeatures<Feature>(kFeatures);

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
