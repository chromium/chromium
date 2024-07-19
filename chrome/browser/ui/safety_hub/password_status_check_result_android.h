// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_ANDROID_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_ANDROID_H_

#include <memory>

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "components/prefs/pref_service.h"

// The result of the periodic password status checks for compromised passwords
// in Clank. This result will be used to show a notifcication on the magic
// stack. Whenever any compromised passwords is detected, the stored number
// should be updated.
class PasswordStatusCheckResultAndroid : public SafetyHubService::Result {
 public:
  PasswordStatusCheckResultAndroid() = delete;

  explicit PasswordStatusCheckResultAndroid(int compromised_passwords_count);

  PasswordStatusCheckResultAndroid(const PasswordStatusCheckResultAndroid&);
  PasswordStatusCheckResultAndroid& operator=(
      const PasswordStatusCheckResultAndroid&);

  ~PasswordStatusCheckResultAndroid() override;

  static std::optional<std::unique_ptr<SafetyHubService::Result>> GetResult(
      const PrefService* pref_service);

  int GetCompromisedPasswordsCount() const {
    return compromised_passwords_count_;
  }

  void UpdateCompromisedPasswordCount(int count);

  // SafetyHubService::Result implementation

  std::unique_ptr<SafetyHubService::Result> Clone() const override;

  base::Value::Dict ToDictValue() const override;

  bool IsTriggerForMenuNotification() const override;

  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const override;

  std::u16string GetNotificationString() const override;

  int GetNotificationCommandId() const override;

 private:
  int compromised_passwords_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_RESULT_ANDROID_H_
