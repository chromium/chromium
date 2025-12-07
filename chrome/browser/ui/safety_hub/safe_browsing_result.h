// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFE_BROWSING_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFE_BROWSING_RESULT_H_

#include <memory>

#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"

class PrefService;

// The state of Safe Browsing settings.
enum class SafeBrowsingState {
  kEnabledEnhanced = 0,
  kEnabledStandard = 1,
  kDisabledByAdmin = 2,
  kDisabledByExtension = 3,
  kDisabledByUser = 4,
  // New enum values must go above here.
  kMaxValue = kDisabledByUser,
};

class SafetyHubSafeBrowsingResult : public SafetyHubResult {
 public:
  SafetyHubSafeBrowsingResult() = delete;

  explicit SafetyHubSafeBrowsingResult(SafeBrowsingState status);

  SafetyHubSafeBrowsingResult(const SafetyHubSafeBrowsingResult&);
  SafetyHubSafeBrowsingResult& operator=(const SafetyHubSafeBrowsingResult&);

  ~SafetyHubSafeBrowsingResult() override;

  static std::optional<std::unique_ptr<SafetyHubResult>> GetResult(
      const PrefService* pref_service);

  static SafeBrowsingState GetState(const PrefService* pref_service);

#if !BUILDFLAG(IS_ANDROID)
  // Fetches data for the Safe Browsing card to return data to the desktop UI.
  static base::Value::Dict GetSafeBrowsingCardData(
      const PrefService* pref_service);
#endif  // BUILDFLAG(IS_ANDROID)

  // SafetyHubResult implementation

  std::unique_ptr<SafetyHubResult> Clone() const override;

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
