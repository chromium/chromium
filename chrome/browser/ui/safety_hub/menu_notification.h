// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_H_

#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

constexpr char kSafetyHubMenuNotificationActiveKey[] = "isCurrentlyActive";
constexpr char kSafetyHubMenuNotificationImpressionCountKey[] =
    "impressionCount";
constexpr char kSafetyHubMenuNotificationFirstImpressionKey[] =
    "firstImpressionTime";
constexpr char kSafetyHubMenuNotificationLastImpressionKey[] =
    "lastImpressionTime";
constexpr char kSafetyHubMenuNotificationResultKey[] = "result";

// Class that represents the notifications of Safety Hub that are shown in the
// Chrome menu.
class SafetyHubMenuNotification {
 public:
  SafetyHubMenuNotification();
  explicit SafetyHubMenuNotification(const base::Value::Dict& dict);

  SafetyHubMenuNotification(const SafetyHubMenuNotification&) = delete;
  SafetyHubMenuNotification& operator=(const SafetyHubMenuNotification&) =
      delete;

  ~SafetyHubMenuNotification();

  base::Value::Dict ToDictValue() const;

  template <typename T>
  static std::unique_ptr<SafetyHubMenuNotification> FromDictValue(
      const base::Value::Dict& dict) {
    auto notification = std::make_unique<SafetyHubMenuNotification>(dict);
    if (dict.contains(kSafetyHubMenuNotificationResultKey)) {
      notification->result_ = SafetyHubService::Result::FromDictValue<T>(
          *dict.FindDict(kSafetyHubMenuNotificationResultKey));
    }
    return notification;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SafetyHubMenuNotificationTest, ToFromDictValue);
  friend class SafetyHubMenuNotificationTest;

  // Indicates whether the notification is actively being shown.
  bool is_currently_active_ = false;
  // Represents how often the notification has been shown to the user.
  int impression_count_ = 0;
  // The first time that the notification was shown to the user, will be nullopt
  // when the notification has never been shown.
  absl::optional<base::Time> first_impression_time_;
  // Similar, but the last time that the notification was shown.
  absl::optional<base::Time> last_impression_time_;
  // The result for which the notification may be shown.
  std::unique_ptr<SafetyHubService::Result> result_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_H_
