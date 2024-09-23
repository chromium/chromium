// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_H_

#include <set>
#include <string>

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

struct PasswordPair {
  std::string origin;
  std::string username;

  auto operator<=>(const PasswordPair&) const = default;
};

// The result of the periodic password status checks for weak, unused and
// compromised passwords. This result will be used to show a notifcication on
// the three dot menu. Whenever any compromised passwords is detected, the
// origin should be added here.
class PasswordStatusCheckResult : public SafetyHubService::Result {
 public:
  PasswordStatusCheckResult();

  PasswordStatusCheckResult(const PasswordStatusCheckResult&);
  PasswordStatusCheckResult& operator=(const PasswordStatusCheckResult&);

  ~PasswordStatusCheckResult() override;

  const std::set<PasswordPair>& GetCompromisedPasswords() const {
    return compromised_passwords_;
  }

  void AddToCompromisedPasswords(std::string origin, std::string username);

  // SafetyHubService::Result implementation

  std::unique_ptr<SafetyHubService::Result> Clone() const override;

  base::Value::Dict ToDictValue() const override;

  bool IsTriggerForMenuNotification() const override;

  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const override;

  std::u16string GetNotificationString() const override;

  int GetNotificationCommandId() const override;

 private:
  std::set<PasswordPair> compromised_passwords_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_H_
