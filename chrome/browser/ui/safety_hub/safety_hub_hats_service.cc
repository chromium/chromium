// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"

#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_result.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_result.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"

SafetyHubHatsService::SafetyHubHatsService(
    TrustSafetySentimentService* tss_service,
    HatsService* hats_service,
    Profile& profile)
    : profile_(profile),
      tss_service_(tss_service),
      hats_service_(hats_service) {}

void SafetyHubHatsService::SafetyHubModuleInteracted() {
  has_interacted_with_module_ = true;
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubInteracted);
}

void SafetyHubHatsService::SafetyHubNotificationClicked(
    std::optional<safety_hub::SafetyHubModuleType> sh_module) {
  has_clicked_notification_ = true;
  last_module_clicked_ = sh_module;
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubInteracted);
}

void SafetyHubHatsService::SafetyHubVisited() {
  has_visited_ = true;
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubInteracted);
}

void SafetyHubHatsService::SafetyHubNotificationSeen() {
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubNotification);
}

void SafetyHubHatsService::TriggerTrustSafetySentimentSurvey(
    TrustSafetySentimentService::FeatureArea area) {
  if (tss_service_) {
    tss_service_->TriggerSafetyHubSurvey(area,
                                         GetSafetyHubProductSpecificData());
  }
}

std::map<std::string, bool>
SafetyHubHatsService::GetSafetyHubProductSpecificData() {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["User visited Safety Hub page"] = has_visited_;
  product_specific_data["User clicked Safety Hub notification"] =
      has_clicked_notification_;
  product_specific_data["User interacted with Safety Hub"] =
      has_interacted_with_module_;

  static constexpr std::array<
      std::pair<safety_hub::SafetyHubModuleType, std::string_view>, 5>
      modules{
          std::make_pair(safety_hub::SafetyHubModuleType::EXTENSIONS,
                         "extensions"),
          {safety_hub::SafetyHubModuleType::NOTIFICATION_PERMISSIONS,
           "notification permissions"},
          {safety_hub::SafetyHubModuleType::PASSWORDS, "passwords"},
          {safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS,
           "revoked permissions"},
          {safety_hub::SafetyHubModuleType::SAFE_BROWSING, "safe browsing"},
      };
  for (const auto& module : modules) {
    product_specific_data[base::StrCat(
        {"Is notification module ", std::get<1>(module)})] =
        last_module_clicked_.has_value() &&
        last_module_clicked_.value() == std::get<0>(module);
  }
  safety_hub::SafetyHubCardState card_state = GetOverallState();
  static constexpr std::array<
      std::pair<safety_hub::SafetyHubCardState, std::string_view>, 4>
      states{
          std::make_pair(safety_hub::SafetyHubCardState::kSafe, "safe"),
          {safety_hub::SafetyHubCardState::kInfo, "info"},
          {safety_hub::SafetyHubCardState::kWeak, "weak"},
          {safety_hub::SafetyHubCardState::kWarning, "warning"},
      };
  for (const auto& state : states) {
    product_specific_data[base::StrCat(
        {"Global state is ", std::get<1>(state)})] =
        card_state == std::get<0>(state);
  }

  return product_specific_data;
}

safety_hub::SafetyHubCardState SafetyHubHatsService::GetOverallState() {
  // If there are any modules that need to be reviewed, the overall state is
  // "warning".
  RevokedPermissionsService* rp_service =
      RevokedPermissionsServiceFactory::GetForProfile(&profile_.get());
  std::optional<std::unique_ptr<SafetyHubResult>> opt_usp_result =
      rp_service->GetCachedResult();
  if (opt_usp_result.has_value()) {
    auto* result =
        static_cast<RevokedPermissionsResult*>(opt_usp_result.value().get());
    if (!result->GetRevokedOrigins().empty()) {
      return safety_hub::SafetyHubCardState::kWarning;
    }
  }

  NotificationPermissionsReviewService* npr_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(
          &profile_.get());
  std::optional<std::unique_ptr<SafetyHubResult>> opt_npr_result =
      npr_service->GetCachedResult();
  if (opt_npr_result.has_value()) {
    auto* result = static_cast<NotificationPermissionsReviewResult*>(
        opt_npr_result.value().get());
    if (!result->GetSortedNotificationPermissions().empty()) {
      return safety_hub::SafetyHubCardState::kWarning;
    }
  }

  std::optional<std::unique_ptr<SafetyHubResult>> opt_ext_result =
      SafetyHubExtensionsResult::GetResult(&profile_.get(), true);
  if (opt_ext_result.has_value()) {
    auto* result =
        static_cast<SafetyHubExtensionsResult*>(opt_ext_result.value().get());
    if (result->GetNumTriggeringExtensions() > 0) {
      return safety_hub::SafetyHubCardState::kWarning;
    }
  }

  // Get the card data for all remaining modules (Chrome Version, Password
  // Status Check, Safe Browsing).
  std::vector<base::Value::Dict> cards;
  cards.push_back(safety_hub_util::GetVersionCardData());

  PasswordStatusCheckService* psc_service =
      PasswordStatusCheckServiceFactory::GetForProfile(&profile_.get());
  CHECK(psc_service);
  cards.push_back(psc_service->GetPasswordCardData());

  cards.push_back(SafetyHubSafeBrowsingResult::GetSafeBrowsingCardData(
      profile_->GetPrefs()));

  // Return the lowest value, which coincides with the "worst" global state.
  safety_hub::SafetyHubCardState min_value =
      safety_hub::SafetyHubCardState::kMaxValue;

  for (const auto& card : cards) {
    safety_hub::SafetyHubCardState current =
        static_cast<safety_hub::SafetyHubCardState>(
            card.FindInt(safety_hub::kCardStateKey).value());
    if (current < min_value) {
      min_value = current;
    }
  }

  return min_value;
}
