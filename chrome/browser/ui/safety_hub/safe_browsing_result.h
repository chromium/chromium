// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFE_BROWSING_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFE_BROWSING_RESULT_H_

#include <memory>

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"

class SafetyHubSafeBrowsingResult : public SafetyHubService::Result {
 public:
  SafetyHubSafeBrowsingResult() = delete;

  explicit SafetyHubSafeBrowsingResult(SafeBrowsingState status);

  SafetyHubSafeBrowsingResult(const SafetyHubSafeBrowsingResult&);
  SafetyHubSafeBrowsingResult& operator=(const SafetyHubSafeBrowsingResult&);

  ~SafetyHubSafeBrowsingResult() override;

  static std::optional<std::unique_ptr<SafetyHubService::Result>> GetResult(
      const PrefService* pref_service);

  static SafeBrowsingState GetState(const PrefService* pref_service);

  // SafetyHubService::Result implementation

  std::unique_ptr<SafetyHubService::Result> Clone() const override;

  base::Value::Dict ToDictValue() const override;

  bool IsTriggerForMenuNotification() const override;

  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const override;

  std::u16string GetNotificationString() const override;

  int GetNotificationCommandId() const override;

 private:
  SafeBrowsingState status_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFE_BROWSING_RESULT_H_
