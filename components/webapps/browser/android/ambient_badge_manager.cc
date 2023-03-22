// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/ambient_badge_manager.h"

#include <limits>
#include <string>

#include "base/feature_list.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/messages/android/messages_feature.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/ambient_badge_metrics.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_infobar_delegate.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
using base::android::JavaParamRef;

namespace webapps {

AmbientBadgeManager::AmbientBadgeManager(
    content::WebContents* web_contents,
    base::WeakPtr<AppBannerManagerAndroid> app_banner_manager)
    : web_contents_(web_contents->GetWeakPtr()),
      app_banner_manager_(app_banner_manager) {}

AmbientBadgeManager::~AmbientBadgeManager() = default;

AmbientBadgeManager::State AmbientBadgeManager::GetStatus() const {
  return badge_state_;
}

void AmbientBadgeManager::MaybeShow(
    const GURL& validated_url,
    const std::u16string& app_name,
    std::unique_ptr<AddToHomescreenParams> a2hs_params,
    base::OnceClosure show_banner_callback) {
  validated_url_ = validated_url;
  app_name_ = app_name;
  a2hs_params_ = std::move(a2hs_params);
  show_banner_callback_ = std::move(show_banner_callback);
  MaybeShowAmbientBadge();
}

void AmbientBadgeManager::AddToHomescreenFromBadge() {
  RecordAmbientBadgeClickEvent(a2hs_params_->app_type);
  std::move(show_banner_callback_).Run();
}

void AmbientBadgeManager::BadgeDismissed() {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents_.get(), validated_url_, a2hs_params_->GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      AppBannerManager::GetCurrentTime());

  RecordAmbientBadgeDismissEvent(a2hs_params_->app_type);
  UpdateState(State::DISMISSED);
}

void AmbientBadgeManager::HideAmbientBadge() {
  message_controller_.DismissMessage();
  infobars::ContentInfoBarManager* infobar_manager =
      webapps::WebappsClient::Get()->GetInfoBarManagerForWebContents(
          web_contents_.get());
  if (infobar_manager == nullptr) {
    return;
  }

  infobars::InfoBar* ambient_badge_infobar =
      InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
          infobar_manager);

  if (ambient_badge_infobar) {
    infobar_manager->RemoveInfoBar(ambient_badge_infobar);
  }
}

void AmbientBadgeManager::OnWorkerCheckResult(const InstallableData& data) {
  if (!data.NoBlockingErrors()) {
    return;
  }
  passed_worker_check_ = true;

  if (badge_state_ == State::PENDING_WORKER) {
    CheckEngagementForAmbientBadge();
  }
}

void AmbientBadgeManager::UpdateState(State state) {
  badge_state_ = state;
}

void AmbientBadgeManager::MaybeShowAmbientBadge() {
  if (!base::FeatureList::IsEnabled(
          features::kInstallableAmbientBadgeInfoBar) &&
      !base::FeatureList::IsEnabled(
          features::kInstallableAmbientBadgeMessage)) {
    return;
  }

  UpdateState(State::ACTIVE);

  // Do not show the ambient badge if it was recently dismissed.
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents_.get(), validated_url_, a2hs_params_->GetAppIdentifier(),
          AppBannerManager::GetCurrentTime())) {
    UpdateState(State::BLOCKED);
    return;
  }

  // if it's showing for web app (not native app), only show if the worker check
  // already passed.
  if (a2hs_params_->app_type == AddToHomescreenParams::AppType::WEBAPK &&
      features::SkipServiceWorkerForInstallPromotion() &&
      !passed_worker_check_) {
    UpdateState(State::PENDING_WORKER);
    PerformWorkerCheckForAmbientBadge();
    return;
  }
  CheckEngagementForAmbientBadge();
}

void AmbientBadgeManager::CheckEngagementForAmbientBadge() {
  if (ShouldSuppressAmbientBadge()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  if (base::FeatureList::IsEnabled(features::kAmbientBadgeSiteEngagement) &&
      !HasSufficientEngagementForAmbientBadge()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  infobars::ContentInfoBarManager* infobar_manager =
      webapps::WebappsClient::Get()->GetInfoBarManagerForWebContents(
          web_contents_.get());
  bool infobar_visible =
      infobar_manager &&
      InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
          infobar_manager);

  if (infobar_visible || message_controller_.IsMessageEnqueued()) {
    return;
  }

  ShowAmbientBadge();
}

void AmbientBadgeManager::PerformWorkerCheckForAmbientBadge() {
  // TODO(crbug/1425546): Move the worker check logic from AppBannerManager.
  app_banner_manager_->PerformWorkerCheckForAmbientBadge();
}

bool AmbientBadgeManager::HasSufficientEngagementForAmbientBadge() {
  // TODO(crbug/1425546): Move the check engagement logic from AppBannerManager.
  return app_banner_manager_->HasSufficientEngagementForAmbientBadge();
}

bool AmbientBadgeManager::ShouldSuppressAmbientBadge() {
  if (!base::FeatureList::IsEnabled(
          features::kAmbientBadgeSuppressFirstVisit)) {
    return false;
  }

  absl::optional<base::Time> last_could_show_time =
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents_.get(), validated_url_, a2hs_params_->GetAppIdentifier(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW_AMBIENT_BADGE);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents_.get(), validated_url_, a2hs_params_->GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW_AMBIENT_BADGE,
      AppBannerManager::GetCurrentTime());

  if (!last_could_show_time || last_could_show_time->is_null()) {
    return true;
  }

  base::TimeDelta period =
      features::kAmbientBadgeSuppressFirstVisit_Period.Get();
  return AppBannerManager::GetCurrentTime() - *last_could_show_time > period;
}

void AmbientBadgeManager::ShowAmbientBadge() {
  RecordAmbientBadgeDisplayEvent(a2hs_params_->app_type);
  UpdateState(State::SHOWING);

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
        a2hs_params_->has_maskable_primary_icon, url);
  } else {
    InstallableAmbientBadgeInfoBarDelegate::Create(
        web_contents_.get(), weak_factory_.GetWeakPtr(), app_name_,
        a2hs_params_->primary_icon, a2hs_params_->has_maskable_primary_icon,
        url);
  }
}

}  // namespace webapps
