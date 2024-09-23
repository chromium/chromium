// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

constexpr base::TimeDelta kSafetyHubMenuNotificationMinNotificationDuration =
    base::Days(3);
constexpr int kSafetyHubMenuNotificationMinImpressionCount = 5;

// Class that represents the notifications of Safety Hub that are shown in the
// Chrome menu.
class SafetyHubMenuNotification {
 public:
  SafetyHubMenuNotification() = delete;
  explicit SafetyHubMenuNotification(safety_hub::SafetyHubModuleType type);
  explicit SafetyHubMenuNotification(const base::Value::Dict& dict,
                                     safety_hub::SafetyHubModuleType type);

  SafetyHubMenuNotification(const SafetyHubMenuNotification&) = delete;
  SafetyHubMenuNotification& operator=(const SafetyHubMenuNotification&) =
      delete;

  ~SafetyHubMenuNotification();

  base::Value::Dict ToDictValue() const;

  // Called when the menu notification will be shown. This will make the
  // notification the currently active one.
  void Show();

  // Called when an active menu notification should no longer be shown, e.g.,
  // when it has been shown a sufficient number of times.
  void Dismiss();

  // Determines whether the notification should be shown given the maximum
  // interval at which this type of notification should be shown. If the
  // provided maximum number of all-time impressions is not 0, this will also be
  // taken into consideration. This method does not take into account whether
  // the notification is currently active.
  bool ShouldBeShown(base::TimeDelta interval,
                     int max_all_time_impressions = 0) const;

  // Returns whether the notification is actively being shown.
  bool IsCurrentlyActive() const;

  // Called whenever a new result for this class of menu notification is
  // available. If the updated result is similar to the current one, no changes
  // are made. Otherwise, the menu notification will be considered as a new one.
  void UpdateResult(std::unique_ptr<SafetyHubService::Result> result);

  // Sets the time at which a notification can start to be shown.
  void SetOnlyShowAfter(base::Time time);

  // Resets the all-time counter for the number of notifications that have ever
  // been shown.
  void ResetAllTimeNotificationCount();

  // Returns the notification string that will be shown in the three-dot menu.
  std::u16string GetNotificationString() const;

  // Returns the Command ID of the notification that will be shown in the
  // three-dot menu.
  int GetNotificationCommandId() const;

  // Returns the module type this menu notification is for.
  safety_hub::SafetyHubModuleType GetModuleType() const;

  SafetyHubService::Result* GetResultForTesting() const;

  // Returns whether any notification for the same type of result has been
  // shown.
  bool HasAnyNotificationBeenShown() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SafetyHubMenuNotificationTest, ToFromDictValue);
  friend class SafetyHubMenuNotificationTest;

  // Used to determine whether the notification has been shown enough times and
  // for a long enough period.
  bool IsShownEnough() const;
  // For the given interval, it is determined whether a notification of the same
  // type can be shown again.
  bool HasIntervalPassed(base::TimeDelta interval) const;

  // Indicates whether the notification is actively being shown.
  bool is_currently_active_ = false;
  // Determines whether the notification should be shown as soon as the interval
  // has passed.
  bool should_be_shown_after_interval_ = false;
  // Represents how often the notification has been shown to the user.
  int impression_count_ = 0;
  // The first time that the notification was shown to the user, will be nullopt
  // when the notification has never been shown.
  std::optional<base::Time> first_impression_time_;
  // Indicates the last time that a notification was shown, even when it is
  // related to a different result.
  std::optional<base::Time> last_impression_time_;
  // The result for which the notification may be shown. Initially, this is a
  // nullptr but its value is updated before any notification will be shown.
  std::unique_ptr<SafetyHubService::Result> current_result_ = nullptr;
  // The previous result that was persisted on disk. This will only be set when
  // the menu notification is created from a Dict value (originating from
  // prefs).
  base::Value::Dict prev_stored_result_;
  // Menu notifications should only be shown after this time.
  std::optional<base::Time> show_only_after_;
  // The total number of time in total that a notification has been shown.
  int all_time_notification_count_ = 0;
  // The type of the module this menu notification is for.
  safety_hub::SafetyHubModuleType module_type_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_H_
