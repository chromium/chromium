// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/ambient_badge_manager.h"

#include <limits>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/android/install_prompt_prefs.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"

namespace webapps {

namespace {

constexpr char kSegmentationResultHistogramName[] =
    "WebApk.InstallPrompt.SegmentationResult";
constexpr char kAmbientBadgeTerminateHistogram[] =
    "Webapp.AmbientBadge.Terminate";

// This enum is used to back UMA histograms, Entries should not be renumbered
// and numeric values should never be reused.
enum class SegmentationResult {
  kInvalid = 0,
  kDontShow = 1,
  kShowInstallPrompt = 2,
  kMaxValue = kShowInstallPrompt,
};

bool gOverrideSegmentationResultForTesting = false;
bool gShowInstallPromptForTesting = false;

}  // namespace

AmbientBadgeManager::AmbientBadgeManager(
    content::WebContents& web_contents,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    PrefService& prefs)
    : web_contents_(web_contents),
      segmentation_platform_service_(segmentation_platform_service),
      pref_service_(prefs) {}

AmbientBadgeManager::~AmbientBadgeManager() {
  base::UmaHistogramEnumeration(kAmbientBadgeTerminateHistogram, state_);
}

void AmbientBadgeManager::MaybeShow(
    const GURL& validated_url,
    const std::u16string& app_name,
    const std::string& app_identifier,
    std::unique_ptr<AddToHomescreenParams> a2hs_params,
    base::OnceClosure show_banner_callback,
    MaybeShowPwaBottomSheetCallback maybe_show_pwa_bottom_sheet) {
  validated_url_ = validated_url;
  app_name_ = app_name;
  app_identifier_ = app_identifier;
  a2hs_params_ = std::move(a2hs_params);
  show_banner_callback_ = std::move(show_banner_callback);
  maybe_show_pwa_bottom_sheet_ = std::move(maybe_show_pwa_bottom_sheet);

  UpdateState(State::kActive);
  if (base::FeatureList::IsEnabled(
          features::kWebAppsEnableMLModelForPromotion)) {
    MaybeShowAmbientBadgeSmart();
  }
}

void AmbientBadgeManager::AddToHomescreenFromBadge() {
  CHECK(a2hs_params_);
  InstallPromptPrefs::RecordInstallPromptClicked(pref_service());
  std::move(show_banner_callback_).Run();
}

void AmbientBadgeManager::BadgeDismissed() {
  CHECK(a2hs_params_);
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, app_identifier_,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      AppBannerManager::GetCurrentTime());

  InstallPromptPrefs::RecordInstallPromptDismissed(
      pref_service(), AppBannerManager::GetCurrentTime());
  UpdateState(State::kDismissed);
}

void AmbientBadgeManager::BadgeIgnored() {
  CHECK(validated_url_.is_valid());
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, app_identifier_,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      AppBannerManager::GetCurrentTime());

  InstallPromptPrefs::RecordInstallPromptIgnored(
      pref_service(), AppBannerManager::GetCurrentTime());
  UpdateState(State::kDismissed);
}

void AmbientBadgeManager::HideAmbientBadge() {
  message_controller_.DismissMessage();
}

void AmbientBadgeManager::UpdateState(State state) {
  state_ = state;
}

void AmbientBadgeManager::MaybeShowAmbientBadgeSmart() {
  if (ShouldMessageBeBlockedByGuardrail()) {
    UpdateState(State::kBlocked);
    return;
  }

  if (!segmentation_platform_service_) {
    return;
  }

  CHECK(validated_url_.is_valid());
  CHECK(a2hs_params_);

  UpdateState(State::kPendingSegmentation);

  if (gOverrideSegmentationResultForTesting) {
    segmentation_platform::ClassificationResult result(
        segmentation_platform::PredictionStatus::kSucceeded);
    result.ordered_labels.emplace_back(
        gShowInstallPromptForTesting
            ? MLInstallabilityPromoter::kShowInstallPromptLabel
            : MLInstallabilityPromoter::kDontShowLabel);
    OnGotClassificationResult(result);
    return;
  }

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
    UMA_HISTOGRAM_ENUMERATION(kSegmentationResultHistogramName,
                              SegmentationResult::kInvalid,
                              SegmentationResult::kMaxValue);
    UpdateState(State::kSegmentationBlock);
    return;
  }

  bool show = !result.ordered_labels.empty() &&
              result.ordered_labels[0] ==
                  MLInstallabilityPromoter::kShowInstallPromptLabel;

  UMA_HISTOGRAM_ENUMERATION(kSegmentationResultHistogramName,
                            show ? SegmentationResult::kShowInstallPrompt
                                 : SegmentationResult::kDontShow,
                            SegmentationResult::kMaxValue);

  if (!show) {
    UpdateState(State::kSegmentationBlock);
    return;
  }

  ShowAmbientBadge();
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
          pref_service(), AppBannerManager::GetCurrentTime())) {
    return true;
  }

  if (InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(
          pref_service(), AppBannerManager::GetCurrentTime())) {
    return true;
  }

  return false;
}

void AmbientBadgeManager::ShowAmbientBadge() {
  if (message_controller_.IsMessageEnqueued()) {
    return;
  }

  UpdateState(State::kShowing);

  WebappInstallSource install_source = InstallableMetrics::GetInstallSource(
      web_contents(), InstallTrigger::AMBIENT_BADGE);
  // TODO(crbug.com/40260952): Move the maybe show peeked bottom sheet logic out
  // of AppBannerManager.
  if (!maybe_show_pwa_bottom_sheet_.is_null() &&
      std::move(maybe_show_pwa_bottom_sheet_).Run(install_source)) {
    // Bottom sheet shown.
    return;
  }

  GURL url = a2hs_params_->app_type == AddToHomescreenParams::AppType::WEBAPK
                 ? a2hs_params_->shortcut_info->url
                 : validated_url_;
  message_controller_.EnqueueMessage(
      web_contents(), app_name_, a2hs_params_->primary_icon,
      a2hs_params_->HasMaskablePrimaryIcon(), url);
}

// static
void AmbientBadgeManager::SetOverrideSegmentationResultForTesting(bool show) {
  gOverrideSegmentationResultForTesting = true;
  gShowInstallPromptForTesting = show;
}

}  // namespace webapps
