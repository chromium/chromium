// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/tips_notifications_ranker.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for TipsNotificationsRanker model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_TIPS_NOTIFICATIONS_RANKER;
// Update the model to include the recent tabs feature.
constexpr int64_t kModelVersion = 9;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;

// Labels for the classification output.
constexpr LabelPair<TipsNotificationsRanker::Label> kTipsNotificationsLabels[] =
    {{TipsNotificationsRanker::kEnhancedSafeBrowsingTipIdx,
      kEnhancedSafeBrowsing},
     {TipsNotificationsRanker::kQuickDeleteTipIdx, kQuickDelete},
     {TipsNotificationsRanker::kGoogleLensTipIdx, kGoogleLens},
     {TipsNotificationsRanker::kBottomOmniboxTipIdx, kBottomOmnibox},
     {TipsNotificationsRanker::kPasswordAutofillTipIdx, kPasswordAutofill},
     {TipsNotificationsRanker::kSigninTipIdx, kSignin},
     {TipsNotificationsRanker::kCreateTabGroupsTipIdx, kCreateTabGroups},
     {TipsNotificationsRanker::kCustomizeMVTTipIdx, kCustomizeMVT},
     {TipsNotificationsRanker::kRecentTabsTipIdx, kRecentTabs}};

// Enum values for histograms.
constexpr std::array<int32_t, 1> kEnumValueForQuickDeleteMagicStackImpression{
    /*QuickDelete=*/9};

constexpr std::array<int32_t, 1> kEnumValueForAllTipsNotificationsShownCount{
    /*Shown=*/3};

constexpr std::array<int32_t, 1> kEnumValueForSigninMagicStackImpression{
    /*Signin=*/15};

constexpr std::array<int32_t, 1> kEnumValueForTabGroupsCreatedCount{
    /*Local=*/1};

// The features here should be in the same order as defined in the header file
// tips_notifications_ranker.h Feature enum.
constexpr FeaturePair<TipsNotificationsRanker::Feature>
    kTipsNotificationsRankerFeatures[] = {
        // V1 Tips: ESB, Quick Delete, Google Lens, Bottom Omnibox
        {TipsNotificationsRanker::kEnhancedSafeBrowsingUseCountIdx,
         features::UserAction("SafeBrowsing.Settings.EnhancedProtectionClicked",
                              28)},
        {TipsNotificationsRanker::kQuickDeleteMagicStackShownCountIdx,
         features::UMAEnum("MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
                           28,
                           kEnumValueForQuickDeleteMagicStackImpression)},
        {TipsNotificationsRanker::kGoogleLensNewTabPageUseCountIdx,
         features::UserAction("NewTabPage.SearchBox.Lens", 28)},
        {TipsNotificationsRanker::kGoogleLensMobileOmniboxUseCountIdx,
         features::UserAction("MobileOmniboxLens", 28)},
        {TipsNotificationsRanker::kGoogleLensTasksSurfaceUseCountIdx,
         features::UserAction("TasksSurface.FakeBox.Lens", 28)},
        {TipsNotificationsRanker::kGoogleLensTipsNotificationsUseCountIdx,
         features::UserAction("Notifications.Tips.Lens", 28)},
        {TipsNotificationsRanker::kEnhancedSafeBrowsingIsEnabledIdx,
         features::InputContext(kEnhancedSafeBrowsingStatus)},
        {TipsNotificationsRanker::kQuickDeleteWasEverUsedIdx,
         features::InputContext(kQuickDeleteUsage)},
        {TipsNotificationsRanker::kBottomOmniboxIsEnabledIdx,
         features::InputContext(kBottomOmniboxStatus)},
        {TipsNotificationsRanker::kBottomOmniboxWasEverUsedIdx,
         features::InputContext(kBottomOmniboxUsage)},
        {TipsNotificationsRanker::kAllFeatureTipsShownCountIdx,
         features::UMAEnum(
             "Notifications.Scheduler.NotificationLifeCycleEvent.Tips",
             7,
             kEnumValueForAllTipsNotificationsShownCount)},
        {TipsNotificationsRanker::kEnhancedSafeBrowsingTipShownIdx,
         features::InputContext(kEnhancedSafeBrowsingTipShown)},
        {TipsNotificationsRanker::kQuickDeleteTipShownIdx,
         features::InputContext(kQuickDeleteTipShown)},
        {TipsNotificationsRanker::kGoogleLensTipShownIdx,
         features::InputContext(kGoogleLensTipShown)},
        {TipsNotificationsRanker::kBottomOmniboxTipShownIdx,
         features::InputContext(kBottomOmniboxTipShown)},
        // V2 Tips: PW Autofill, Signin, Tab Groups, Customize MVT, Recent Tabs
        // Check that both the synced account and local passwords count have an
        // aggregate sum of 0 during the specified window across all instances.
        {TipsNotificationsRanker::kPasswordAutofillAccountPasswordsCountIdx,
         features::UMASum(
             "PasswordManager.AccountStore.TotalAccountsHiRes3.ByType.Overall",
             28)},
        {TipsNotificationsRanker::kPasswordAutofillLocalPasswordsCountIdx,
         features::UMASum(
             "PasswordManager.ProfileStore.TotalAccountsHiRes3.ByType.Overall",
             28)},
        {TipsNotificationsRanker::kIsUserSignedInIdx,
         features::InputContext(kTipsIsUserSignedIn)},
        {TipsNotificationsRanker::kSigninMagicStackShownCountIdx,
         features::UMAEnum("MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
                           28,
                           kEnumValueForSigninMagicStackImpression)},
        {TipsNotificationsRanker::kTabGroupsCreatedCountIdx,
         features::UMAEnum("TabGroups.Sync.TabGroup.Created.GroupCreateOrigin",
                           28,
                           kEnumValueForTabGroupsCreatedCount)},
        {TipsNotificationsRanker::kNTPShownCountIdx,
         features::UserAction("MobileNTPShown", 7)},
        {TipsNotificationsRanker::kMVTPinnedCountIdx,
         features::UserAction("Suggestions.ContextMenu.PinItem", 28)},
        {TipsNotificationsRanker::kRecentTabsUsedCountIdx,
         features::UserAction("MobileMenuRecentTabs", 28)},
        {TipsNotificationsRanker::kPasswordAutofillTipShownIdx,
         features::InputContext(kPasswordAutofillTipShown)},
        {TipsNotificationsRanker::kSigninTipShownIdx,
         features::InputContext(kSigninTipShown)},
        {TipsNotificationsRanker::kCreateTabGroupsTipShownIdx,
         features::InputContext(kCreateTabGroupsTipShown)},
        {TipsNotificationsRanker::kCustomizeMVTTipShownIdx,
         features::InputContext(kCustomizeMVTTipShown)},
        {TipsNotificationsRanker::kRecentTabsTipShownIdx,
         features::InputContext(kRecentTabsTipShown)}};

std::vector<int> GetTipsPriorityRankingList() {
  std::vector<int> tips_list;
  // Define the priority ranking based on the feature param.
  // First in the list represents highest priority and last is lowest.
  if (base::FeatureList::IsEnabled(features::kAndroidTipsNotificationsV2)) {
    tips_list.emplace_back(TipsNotificationsRanker::kPasswordAutofillTipIdx);
    tips_list.emplace_back(TipsNotificationsRanker::kSigninTipIdx);
    tips_list.emplace_back(TipsNotificationsRanker::kCreateTabGroupsTipIdx);
    tips_list.emplace_back(TipsNotificationsRanker::kCustomizeMVTTipIdx);
    tips_list.emplace_back(TipsNotificationsRanker::kRecentTabsTipIdx);
  }

  if (features::kTrustAndSafety.Get()) {
    if (features::kEnableEnhancedSafeBrowsingTip.Get()) {
      tips_list.emplace_back(
          TipsNotificationsRanker::kEnhancedSafeBrowsingTipIdx);
    }
    if (features::kEnableQuickDeleteTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kQuickDeleteTipIdx);
    }
    if (features::kEnableGoogleLensTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kGoogleLensTipIdx);
    }
    if (features::kEnableBottomOmniboxTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kBottomOmniboxTipIdx);
    }
  } else if (features::kEssential.Get()) {
    if (features::kEnableQuickDeleteTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kQuickDeleteTipIdx);
    }
    if (features::kEnableBottomOmniboxTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kBottomOmniboxTipIdx);
    }
    if (features::kEnableEnhancedSafeBrowsingTip.Get()) {
      tips_list.emplace_back(
          TipsNotificationsRanker::kEnhancedSafeBrowsingTipIdx);
    }
    if (features::kEnableGoogleLensTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kGoogleLensTipIdx);
    }
  } else if (features::kNewFeatures.Get()) {
    if (features::kEnableGoogleLensTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kGoogleLensTipIdx);
    }
    if (features::kEnableBottomOmniboxTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kBottomOmniboxTipIdx);
    }
    if (features::kEnableQuickDeleteTip.Get()) {
      tips_list.emplace_back(TipsNotificationsRanker::kQuickDeleteTipIdx);
    }
    if (features::kEnableEnhancedSafeBrowsingTip.Get()) {
      tips_list.emplace_back(
          TipsNotificationsRanker::kEnhancedSafeBrowsingTipIdx);
    }
  }
  return tips_list;
}

bool IsEnhancedSafeBrowsingTipEligible(float is_enabled,
                                       float use_count,
                                       float tip_shown) {
  return is_enabled == 0 && use_count == 0 && tip_shown == 0;
}

bool IsQuickDeleteTipEligible(float was_ever_used,
                              float magic_stack_shown_count,
                              float tip_shown) {
  return was_ever_used == 0 && magic_stack_shown_count == 0 && tip_shown == 0;
}

bool IsGoogleLensTipEligible(float ntp_use_count,
                             float omnibox_use_count,
                             float tasks_surface_use_count,
                             float tips_notifications_use_count,
                             float tip_shown) {
  return ntp_use_count == 0 && omnibox_use_count == 0 &&
         tasks_surface_use_count == 0 && tips_notifications_use_count == 0 &&
         tip_shown == 0;
}

bool IsBottomOmniboxTipEligible(float is_enabled,
                                float was_ever_used,
                                float tip_shown) {
  return is_enabled == 0 && was_ever_used == 0 && tip_shown == 0;
}

bool IsPasswordAutofillTipEligible(float account_passwords_count,
                                   float local_passwords_count,
                                   float tip_shown) {
  return account_passwords_count == 0 && local_passwords_count == 0 &&
         tip_shown == 0;
}

bool IsSigninTipEligible(float is_user_signed_in,
                         float magic_stack_shown_count,
                         float tip_shown) {
  return is_user_signed_in == 0 && magic_stack_shown_count == 0 &&
         tip_shown == 0;
}

bool IsCreateTabGroupsTipEligible(float tab_groups_created_count,
                                  float tip_shown) {
  return tab_groups_created_count == 0 && tip_shown == 0;
}

bool IsCustomizeMVTTipEligible(float ntp_shown_count,
                               float mvt_pinned_count,
                               float tip_shown) {
  return ntp_shown_count > 5 && mvt_pinned_count == 0 && tip_shown == 0;
}

bool IsRecentTabsTipEligible(float recent_tabs_used_count, float tip_shown) {
  return recent_tabs_used_count == 0 && tip_shown == 0;
}

}  // namespace

// static
std::unique_ptr<Config> TipsNotificationsRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kAndroidTipsNotifications)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kTipsNotificationsRankerKey;
  config->segmentation_uma_name = kTipsNotificationsRankerUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<TipsNotificationsRanker>());
  config->auto_execute_and_cache = false;
  return config;
}

TipsNotificationsRanker::TipsNotificationsRanker()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
TipsNotificationsRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  metadata.set_upload_tensors(false);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier<Label>(kTipsNotificationsLabels,
                                                       /*threshold=*/0.5);

  // Set UMA features and custom inputs.
  writer.AddFeatures<Feature>(kTipsNotificationsRankerFeatures);

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void TipsNotificationsRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response response(kLabelCount, 0);
  // Counts refer to the L28 days and bools are represented through 0 or 1.
  // TODO(crbug.com/444281425): Include logic for trying to schedule once a week
  // and to cycle the tips via histogram on notif showing for L28 or max 1 time.

  // V1 Tips: ESB, Quick Delete, Google Lens, Bottom Omnibox
  float esb_is_enabled = inputs[kEnhancedSafeBrowsingIsEnabledIdx];
  float esb_use_count = inputs[kEnhancedSafeBrowsingUseCountIdx];
  float qd_ever_used = inputs[kQuickDeleteWasEverUsedIdx];
  float qd_magic_stack_shown_count =
      inputs[kQuickDeleteMagicStackShownCountIdx];
  float lens_ntp_use_count = inputs[kGoogleLensNewTabPageUseCountIdx];
  float lens_omnibox_use_count = inputs[kGoogleLensMobileOmniboxUseCountIdx];
  float lens_tasks_surface_use_count =
      inputs[kGoogleLensTasksSurfaceUseCountIdx];
  float lens_tips_notifications_use_count =
      inputs[kGoogleLensTipsNotificationsUseCountIdx];
  float bottom_omnibox_is_enabled = inputs[kBottomOmniboxIsEnabledIdx];
  float bottom_omnibox_was_ever_used = inputs[kBottomOmniboxWasEverUsedIdx];
  float all_feature_tips_shown_count = inputs[kAllFeatureTipsShownCountIdx];
  float esb_tip_shown = inputs[kEnhancedSafeBrowsingTipShownIdx];
  float qd_tip_shown = inputs[kQuickDeleteTipShownIdx];
  float lens_tip_shown = inputs[kGoogleLensTipShownIdx];
  float bottom_omnibox_tip_shown = inputs[kBottomOmniboxTipShownIdx];

  // V2 Tips: PW Autofill, Signin, Create Tab Groups, Customize MVT, Recent Tabs
  float password_autofill_account_passwords_count =
      inputs[kPasswordAutofillAccountPasswordsCountIdx];
  float password_autofill_local_passwords_count =
      inputs[kPasswordAutofillLocalPasswordsCountIdx];
  float is_user_signed_in = inputs[kIsUserSignedInIdx];
  float signin_magic_stack_shown_count = inputs[kSigninMagicStackShownCountIdx];
  float tab_groups_created_count = inputs[kTabGroupsCreatedCountIdx];
  float ntp_shown_count = inputs[kNTPShownCountIdx];
  float mvt_pinned_count = inputs[kMVTPinnedCountIdx];
  float recent_tabs_used_count = inputs[kRecentTabsUsedCountIdx];
  float password_autofill_tip_shown = inputs[kPasswordAutofillTipShownIdx];
  float signin_tip_shown = inputs[kSigninTipShownIdx];
  float create_tab_groups_tip_shown = inputs[kCreateTabGroupsTipShownIdx];
  float customize_mvt_tip_shown = inputs[kCustomizeMVTTipShownIdx];
  float recent_tabs_tip_shown = inputs[kRecentTabsTipShownIdx];

  // Only choose an eligible tip if none have been shown for the last 7 days or
  // if the testing flags to instantly schedule a notification are active.
  if (all_feature_tips_shown_count == 0 ||
      features::kStartTimeMinutes.Get() < 5) {
    // Cycle through the priority list and mark the highest ranked eligible tip
    // to show if it exists and then early exit.
    std::vector<int> tips_priority_list = GetTipsPriorityRankingList();
    if (!tips_priority_list.empty()) {
      bool has_eligible_tip = false;
      for (auto tip_idx : tips_priority_list) {
        switch (tip_idx) {
          case kEnhancedSafeBrowsingTipIdx:
            if (IsEnhancedSafeBrowsingTipEligible(esb_is_enabled, esb_use_count,
                                                  esb_tip_shown)) {
              response[kEnhancedSafeBrowsingTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kQuickDeleteTipIdx:
            if (IsQuickDeleteTipEligible(
                    qd_ever_used, qd_magic_stack_shown_count, qd_tip_shown)) {
              response[kQuickDeleteTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kGoogleLensTipIdx:
            if (IsGoogleLensTipEligible(
                    lens_ntp_use_count, lens_omnibox_use_count,
                    lens_tasks_surface_use_count,
                    lens_tips_notifications_use_count, lens_tip_shown)) {
              response[kGoogleLensTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kBottomOmniboxTipIdx:
            if (IsBottomOmniboxTipEligible(bottom_omnibox_is_enabled,
                                           bottom_omnibox_was_ever_used,
                                           bottom_omnibox_tip_shown)) {
              response[kBottomOmniboxTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kPasswordAutofillTipIdx:
            if (IsPasswordAutofillTipEligible(
                    password_autofill_account_passwords_count,
                    password_autofill_local_passwords_count,
                    password_autofill_tip_shown)) {
              response[kPasswordAutofillTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kSigninTipIdx:
            if (IsSigninTipEligible(is_user_signed_in,
                                    signin_magic_stack_shown_count,
                                    signin_tip_shown)) {
              response[kSigninTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kCreateTabGroupsTipIdx:
            if (IsCreateTabGroupsTipEligible(tab_groups_created_count,
                                             create_tab_groups_tip_shown)) {
              response[kCreateTabGroupsTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kCustomizeMVTTipIdx:
            if (IsCustomizeMVTTipEligible(ntp_shown_count, mvt_pinned_count,
                                          customize_mvt_tip_shown)) {
              response[kCustomizeMVTTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          case kRecentTabsTipIdx:
            if (IsRecentTabsTipEligible(recent_tabs_used_count,
                                        recent_tabs_tip_shown)) {
              response[kRecentTabsTipIdx] = 1;
              has_eligible_tip = true;
            }
            break;
          default:
            NOTREACHED();
        }

        // Early exit if an eligible tip has been found.
        if (has_eligible_tip) {
          break;
        }
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
