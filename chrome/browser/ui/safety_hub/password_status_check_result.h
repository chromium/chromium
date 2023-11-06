// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_H_

#include <set>
#include <string>

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

inline constexpr char kSafetyHubPasswordCheckOriginsKey[] =
    "passwordCheckOrigins";

// The result of the periodic password status checks for weak, unused and
// compromised passwords. This result will be used to show a notifcication on
// the three dot menu. Whenever any compromised passwords is detected, the
// origin should be added here.
class PasswordStatusCheckResult : public SafetyHubService::Result {
 public:
  PasswordStatusCheckResult();

  // 'dict' should be a base::Value::List that holds the origins as the format
  // below:
  // {kSafetyHubPasswordCheckOriginsKey: ["example1.com", "example2.com"]}
  explicit PasswordStatusCheckResult(const base::Value::Dict& dict);

  PasswordStatusCheckResult(const PasswordStatusCheckResult&);
  PasswordStatusCheckResult& operator=(const PasswordStatusCheckResult&);

  ~PasswordStatusCheckResult() override;

  const std::set<std::string>& GetCompromisedOrigins() const {
    return compromised_origins_;
  }

  void AddToCompromisedOrigins(std::string origin);

  // SafetyHubService::Result implementation

  std::unique_ptr<SafetyHubService::Result> Clone() const override;

  base::Value::Dict ToDictValue() const override;

  bool IsTriggerForMenuNotification() const override;

  bool WarrantsNewMenuNotification(const Result& previousResult) const override;

  std::u16string GetNotificationString() const override;

  int GetNotificationCommandId() const override;

 private:
  std::set<std::string> compromised_origins_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_H_
