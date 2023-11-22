// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/ambient_badge_manager.h"

#include <limits>
#include <string>

#include "base/feature_list.h"
#include "components/messages/android/messages_feature.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/ambient_badge_metrics.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/android/install_prompt_prefs.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webapps {

namespace {

InstallableParams ParamsToPerformWorkerCheck() {
  InstallableParams params;
  params.has_worker = true;
  params.wait_for_worker = true;
  return params;
}

}  // namespace

AmbientBadgeManager::AmbientBadgeManager(
    content::WebContents* web_contents,
    base::WeakPtr<AppBannerManagerAndroid> app_banner_manager,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    PrefService* prefs)
    : web_contents_(web_contents->GetWeakPtr()),
      app_banner_manager_(app_banner_manager),
      segmentation_platform_service_(segmentation_platform_service),
      pref_service_(prefs) {}

AmbientBadgeManager::~AmbientBadgeManager() {
  RecordAmbientBadgeTeminateState(state_);
}

void AmbientBadgeManager::MaybeShow(
    const GURL& validated_url,
    const std::u16string& app_name,
    const std::string& app_identifier,
    std::unique_ptr<AddToHomescreenParams> a2hs_params,
    base::OnceClosure show_banner_callback) {
  validated_url_ = validated_url;
  app_name_ = app_name;
  app_identifier_ = app_identifier;
  a2hs_params_ = std::move(a2hs_params);
  show_banner_callback_ = std::move(show_banner_callback);

  if (!base::FeatureList::IsEnabled(
          features::kInstallableAmbientBadgeMessage)) {
    return;
  }

  UpdateState(State::kActive);

  if (base::FeatureList::IsEnabled(features::kInstallPromptSegmentation)) {
    MaybeShowAmbientBadgeSmart();
  } else {
    MaybeShowAmbientBadgeLegacy();
  }
}

void AmbientBadgeManager::AddToHomescreenFromBadge() {
  RecordAmbientBadgeClickEvent(a2hs_params_->app_type);
  InstallPromptPrefs::RecordInstallPromptClicked(pref_service_);
  std::move(show_banner_callback_).Run();
}

void AmbientBadgeManager::BadgeDismissed() {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents_.get(), validated_url_, app_identifier_,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      AppBannerManager::GetCurrentTime());

  InstallPromptPrefs::RecordInstallPromptDismissed(
      pref_service_, AppBannerManager::GetCurrentTime());
  RecordAmbientBadgeDismissEvent(a2hs_params_->app_type);
  UpdateState(State::kDismissed);
}

void AmbientBadgeManager::BadgeIgnored() {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents_.get(), validated_url_, app_identifier_,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      AppBannerManager::GetCurrentTime());

  InstallPromptPrefs::RecordInstallPromptIgnored(
      pref_service_, AppBannerManager::GetCurrentTime());
  RecordAmbientBadgeDismissEvent(a2hs_params_->app_type);
  UpdateState(State::kDismissed);
}

void AmbientBadgeManager::HideAmbientBadge() {
  message_controller_.DismissMessage();
}

void AmbientBadgeManager::UpdateState(State state) {
  state_ = state;
}

void AmbientBadgeManager::MaybeShowAmbientBadgeLegacy() {
  // Do not show the ambient badge if it was recently dismissed.
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents_.get(), validated_url_, app_identifier_,
          AppBannerManager::GetCurrentTime())) {
    UpdateState(State::kBlocked);
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kBlockInstallPromptIfIgnoreRecently) &&
      AppBannerSettingsHelper::WasBannerRecentlyIgnored(
          web_contents_.get(), validated_url_, app_identifier_,
          AppBannerManager::GetCurrentTime())) {
    UpdateState(State::kBlocked);
    return;
  }

  if (ShouldSuppressAmbientBadgeOnFirstVisit()) {
    UpdateState(State::kPendingEngagement);
    return;
  }

  // if it's showing for web app (not native app), only show if the worker check
  // already passed.
  if (a2hs_params_->app_type == AddToHomescreenParams::AppType::WEBAPK &&
      !passed_worker_check_) {
    PerformWorkerCheckForAmbientBadge(
        ParamsToPerformWorkerCheck(),
        base::BindOnce(&AmbientBadgeManager::OnWorkerCheckResult,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  ShowAmbientBadge();
}

bool AmbientBadgeManager::ShouldSuppressAmbientBadgeOnFirstVisit() {
  if (!base::FeatureList::IsEnabled(
          features::kAmbientBadgeSuppressFirstVisit)) {
    return false;
  }

  absl::optional<base::Time> last_could_show_time =
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents_.get(), validated_url_, app_identifier_,
          AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW_AMBIENT_BADGE);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents_.get(), validated_url_, app_identifier_,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW_AMBIENT_BADGE,
      AppBannerManager::GetCurrentTime());

  if (!last_could_show_time || last_could_show_time->is_null()) {
    return true;
  }

  base::TimeDelta period =
      features::kAmbientBadgeSuppressFirstVisit_Period.Get();
  return AppBannerManager::GetCurrentTime() - *last_could_show_time > period;
}

void AmbientBadgeManager::PerformWorkerCheckForAmbientBadge(
    InstallableParams params,
    InstallableCallback callback) {
  UpdateState(State::kPendingWorker);
  // TODO(crbug/1425546): Move the worker check logic from AppBannerManager.
  app_banner_manager_->PerformWorkerCheckForAmbientBadge(params,
                                                         std::move(callback));
}

void AmbientBadgeManager::OnWorkerCheckResult(const InstallableData& data) {
  if (!data.errors.empty()) {
    return;
  }
  passed_worker_check_ = true;

  if (state_ == State::kPendingWorker) {
    ShowAmbientBadge();
  }
}

void AmbientBadgeManager::MaybeShowAmbientBadgeSmart() {
  if (ShouldMessageBeBlockedByGuardrail()) {
    UpdateState(State::kBlocked);
    return;
  }

  if (!segmentation_platform_service_) {
    return;
  }

  UpdateState(State::kSegmentation);

  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  input_context->metadata_args.emplace("url", validated_url_);
  input_context->metadata_args.emplace(
      "origin", url::Origin::Create(validated_url_).GetURL());
  input_context->metadata_args.emplace(
      "maskable_icon",
      segmentation_platform::processing::ProcessedValue::FromFloat(
          a2hs_params_->HasMaskablePrimaryIcon()));
  input_context->metadata_args.emplace(
      "app_type", segmentation_platform::processing::ProcessedValue::FromFloat(
                      (float)a2hs_params_->app_type));
  segmentation_platform_service_->GetClassificationResult(
      segmentation_platform::kWebAppInstallationPromoKey, prediction_options,
      input_context,
      base::BindOnce(&AmbientBadgeManager::OnGotClassificationResult,
                     weak_factory_.GetWeakPtr()));
}

void AmbientBadgeManager::OnGotClassificationResult(
    const segmentation_platform::ClassificationResult& result) {
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    // If the classification is not ready yet, fallback to the legacy logic.
    MaybeShowAmbientBadgeLegacy();
    return;
  }

  if (!result.ordered_labels.empty() &&
      result.ordered_labels[0] ==
          MLInstallabilityPromoter::kShowInstallPromptLabel) {
      ShowAmbientBadge();
  }
}

bool AmbientBadgeManager::ShouldMessageBeBlockedByGuardrail() {
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents(), validated_url_, app_identifier_,
          AppBannerManager::GetCurrentTime())) {
    return true;
  }

  if (AppBannerSettingsHelper::WasBannerRecentlyIgnored(
          web_contents(), validated_url_, app_identifier_,
          AppBannerManager::GetCurrentTime())) {
    return true;
  }

  if (InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
          pref_service_, AppBannerManager::GetCurrentTime())) {
    return true;
  }

  if (InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(
          pref_service_, AppBannerManager::GetCurrentTime())) {
    return true;
  }

  return false;
}

void AmbientBadgeManager::ShowAmbientBadge() {
  if (message_controller_.IsMessageEnqueued()) {
    return;
  }

  RecordAmbientBadgeDisplayEvent(a2hs_params_->app_type);
  UpdateState(State::kShowing);

  WebappInstallSource install_source = InstallableMetrics::GetInstallSource(
      web_contents_.get(), InstallTrigger::AMBIENT_BADGE);
  // TODO(crbug/1425546): Move the maybe show peeked bottom sheet logic out of
  // AppBannerManager.
  if (app_banner_manager_->MaybeShowPwaBottomSheetController(
          /* expand_sheet= */ false, install_source)) {
    // Bottom sheet shown.
    return;
  }

  GURL url = a2hs_params_->app_type == AddToHomescreenParams::AppType::WEBAPK
                 ? a2hs_params_->shortcut_info->url
                 : validated_url_;
  if (base::FeatureList::IsEnabled(features::kInstallableAmbientBadgeMessage) &&
      base::FeatureList::IsEnabled(
          messages::kMessagesForAndroidInfrastructure)) {
    message_controller_.EnqueueMessage(
        web_contents_.get(), app_name_, a2hs_params_->primary_icon,
        a2hs_params_->HasMaskablePrimaryIcon(), url);
  }
}

}  // namespace webapps
