// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/rand_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"

namespace {

base::TimeDelta GetMinTimeToPrompt() {
  return features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
}

base::TimeDelta GetMaxTimeToPrompt() {
  return features::kTrustSafetySentimentSurveyMaxTimeToPrompt.Get();
}

int GetRequiredNtpCount() {
  return base::RandInt(
      features::kTrustSafetySentimentSurveyNtpVisitsMinRange.Get(),
      features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get());
}

std::string GetHatsTriggerForFeatureArea(
    TrustSafetySentimentService::FeatureArea feature_area) {
  switch (feature_area) {
    case (TrustSafetySentimentService::FeatureArea::kPrivacySettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySettings;
    case (TrustSafetySentimentService::FeatureArea::kTrustedSurface):
      return kHatsSurveyTriggerTrustSafetyTrustedSurface;
    case (TrustSafetySentimentService::FeatureArea::kTransactions):
      return kHatsSurveyTriggerTrustSafetyTransactions;
  }
  NOTREACHED();
  return "";
}

bool ProbabilityCheck(TrustSafetySentimentService::FeatureArea feature_area) {
  switch (feature_area) {
    case (TrustSafetySentimentService::FeatureArea::kPrivacySettings):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyPrivacySettingsProbability
                 .Get();
    case (TrustSafetySentimentService::FeatureArea::kTrustedSurface):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTrustedSurfaceProbability
                 .Get();
    case (TrustSafetySentimentService::FeatureArea::kTransactions):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTransactionsProbability.Get();
  }
  NOTREACHED();
  return false;
}

}  // namespace

TrustSafetySentimentService::TrustSafetySentimentService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
}

TrustSafetySentimentService::~TrustSafetySentimentService() = default;

void TrustSafetySentimentService::OpenedNewTabPage() {
  // Explicit early exit for the common path, where the user has not performed
  // any of the trigger actions.
  if (pending_triggers_.size() == 0)
    return;

  // Remove any triggers which occurred more than the maximum time ago.
  base::EraseIf(pending_triggers_,
                [](const std::pair<FeatureArea, PendingTrigger>& area_trigger) {
                  return base::Time::Now() - area_trigger.second.occurred_time >
                         GetMaxTimeToPrompt();
                });

  // This may have emptied the set of pending triggers.
  if (pending_triggers_.size() == 0)
    return;

  // Reduce the NTPs to open count for all the active triggers.
  for (auto& area_trigger : pending_triggers_) {
    auto& trigger = area_trigger.second;
    if (trigger.remaining_ntps_to_open > 0)
      trigger.remaining_ntps_to_open--;
  }

  // Any trigger being too recent, or not having the required number of NTP
  // opens, will prevent a survey from being shown.
  auto now = base::Time::Now();
  for (const auto& area_trigger : pending_triggers_) {
    const auto& trigger = area_trigger.second;
    if (now - trigger.occurred_time < GetMinTimeToPrompt() ||
        trigger.remaining_ntps_to_open > 0) {
      return;
    }
  }

  // Choose a trigger at random to avoid any order biasing.
  auto winning_area_iterator = pending_triggers_.begin();
  std::advance(winning_area_iterator,
               base::RandInt(0, pending_triggers_.size() - 1));

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  // A null HaTS service should have prevented this service from being created.
  DCHECK(hats_service);
  hats_service->LaunchSurvey(
      GetHatsTriggerForFeatureArea(winning_area_iterator->first),
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      winning_area_iterator->second.product_specific_data);

  pending_triggers_.clear();
}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const std::map<std::string, bool>& product_specific_data,
    int remaining_ntps_to_open)
    : product_specific_data(product_specific_data),
      remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::~PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const PendingTrigger& other) = default;

void TrustSafetySentimentService::TriggerOccurred(
    FeatureArea feature_area,
    const std::map<std::string, bool>& product_specific_data) {
  if (!ProbabilityCheck(feature_area))
    return;

  // This will overwrite any previous trigger for this feature area. We are only
  // interested in the most recent trigger, so this is acceptable.
  pending_triggers_[feature_area] =
      PendingTrigger(product_specific_data, GetRequiredNtpCount());
}
