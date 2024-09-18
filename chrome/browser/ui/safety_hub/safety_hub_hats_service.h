// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_HATS_SERVICE_H_

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class SafetyHubMenuNotificationService;

class SafetyHubHatsService : public KeyedService {
 public:
  SafetyHubHatsService(
      TrustSafetySentimentService* tss_service,
      SafetyHubMenuNotificationService& menu_notification_service,
      Profile& profile);

  SafetyHubHatsService(const SafetyHubHatsService&) = delete;
  SafetyHubHatsService& operator=(const SafetyHubHatsService&) = delete;

  // Called when the user interacts with a module of Safety Hub.
  void SafetyHubModuleInteracted();

  // Called when the user clicks a menu notification from Safety Hub.
  void SafetyHubNotificationClicked();

  // Called when the user visits the Safety Hub page.
  void SafetyHubVisited();

  // Called when the user has seen the menu notification for Safety Hub for at
  // least 5 seconds.
  void SafetyHubNotificationSeen();

  // Returns the product specific data related to surveys triggered for Safety
  // Hub.
  std::map<std::string, bool> GetSafetyHubProductSpecificData();

 private:
  // Triggers a Safety Hub survey for the long-term Trust & Safety sentiment
  // tracking.
  void TriggerTrustSafetySentimentSurvey(
      TrustSafetySentimentService::FeatureArea area);

  // Triggers a Safety Hub survey for the long-term Trust & Safety sentiment
  // tracking.
  void TriggerOneOffSurvey(const std::string& trigger);

  const raw_ref<Profile> profile_;
  const raw_ptr<TrustSafetySentimentService> tss_service_;
  const raw_ref<SafetyHubMenuNotificationService> menu_notification_service_;

  // The different states that represents the Safety Hub state, and more
  // specifically the user's interactions with it.
  bool has_visited_ = false;
  bool has_interacted_with_module_ = false;
  bool has_clicked_notification_ = false;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_HATS_SERVICE_H_
