// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"

#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/safety_hub/card_data_helper.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/common/chrome_features.h"

SafetyHubHatsService::SafetyHubHatsService(
    TrustSafetySentimentService* tss_service,
    SafetyHubMenuNotificationService& menu_notification_service,
    Profile& profile)
    : profile_(profile),
      tss_service_(tss_service),
      menu_notification_service_(menu_notification_service) {}

void SafetyHubHatsService::SafetyHubModuleInteracted() {
  has_interacted_with_module_ = true;
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubInteracted);
  TriggerOneOffSurvey(kHatsSurveyTriggerSafetyHubOneOffExperimentInteraction);
}

void SafetyHubHatsService::SafetyHubNotificationClicked() {
  has_clicked_notification_ = true;
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubInteracted);
  TriggerOneOffSurvey(kHatsSurveyTriggerSafetyHubOneOffExperimentInteraction);
}

void SafetyHubHatsService::SafetyHubVisited() {
  has_visited_ = true;
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubInteracted);
  TriggerOneOffSurvey(kHatsSurveyTriggerSafetyHubOneOffExperimentInteraction);
}

void SafetyHubHatsService::SafetyHubNotificationSeen() {
  TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea::kSafetyHubNotification);
  TriggerOneOffSurvey(kHatsSurveyTriggerSafetyHubOneOffExperimentNotification);
}

void SafetyHubHatsService::TriggerTrustSafetySentimentSurvey(
    TrustSafetySentimentService::FeatureArea area) {
  if (tss_service_) {
    tss_service_->TriggerSafetyHubSurvey(area,
                                         GetSafetyHubProductSpecificData());
  }
}

void SafetyHubHatsService::TriggerOneOffSurvey(const std::string& trigger) {
  if (!base::FeatureList::IsEnabled(features::kSafetyHubHaTSOneOffSurvey)) {
    return;
  }

  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      &*profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
  hats_service->LaunchSurvey(trigger,
                             /*success_callback=*/base::DoNothing(),
                             /*failure_callback=*/base::DoNothing(),
                             GetSafetyHubProductSpecificData());
}

std::map<std::string, bool>
SafetyHubHatsService::GetSafetyHubProductSpecificData() {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["User visited Safety Hub page"] = has_visited_;
  product_specific_data["User clicked Safety Hub notification"] =
      has_clicked_notification_;
  product_specific_data["User interacted with Safety Hub"] =
      has_interacted_with_module_;

  std::optional<safety_hub::SafetyHubModuleType> last_module =
      menu_notification_service_->GetLastShownNotificationModule();

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
        last_module.has_value() && last_module.value() == std::get<0>(module);
  }
  safety_hub::SafetyHubCardState card_state =
      safety_hub::GetOverallState(&*profile_);
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
