// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification.h"

#include <memory>
#include <string>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_result.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"

SafetyHubMenuNotification::SafetyHubMenuNotification(
    safety_hub::SafetyHubModuleType type)
    : first_impression_time_(std::nullopt),
      last_impression_time_(std::nullopt),
      show_only_after_(std::nullopt),
      module_type_(type) {}

SafetyHubMenuNotification::~SafetyHubMenuNotification() = default;

SafetyHubMenuNotification::SafetyHubMenuNotification(
    const base::Value::Dict& dict,
    safety_hub::SafetyHubModuleType type)
    : module_type_(type) {
  is_currently_active_ =
      dict.FindBool(safety_hub::kSafetyHubMenuNotificationActiveKey).value();
  impression_count_ =
      dict.FindInt(safety_hub::kSafetyHubMenuNotificationImpressionCountKey)
          .value_or(0);
  all_time_notification_count_ =
      dict.FindInt(safety_hub::kSafetyHubMenuNotificationAllTimeCountKey)
          .value_or(0);
  first_impression_time_ = base::ValueToTime(
      dict.Find(safety_hub::kSafetyHubMenuNotificationFirstImpressionKey));
  last_impression_time_ = base::ValueToTime(
      dict.Find(safety_hub::kSafetyHubMenuNotificationLastImpressionKey));
  show_only_after_ = base::ValueToTime(
      dict.Find(safety_hub::kSafetyHubMenuNotificationShowAfterTimeKey));
  if (dict.contains(safety_hub::kSafetyHubMenuNotificationResultKey)) {
    prev_stored_result_ =
        dict.FindDict(safety_hub::kSafetyHubMenuNotificationResultKey)->Clone();
  }
}

base::Value::Dict SafetyHubMenuNotification::ToDictValue() const {
  base::Value::Dict result;
  result.Set(safety_hub::kSafetyHubMenuNotificationActiveKey,
             is_currently_active_);
  if (impression_count_ != 0) {
    result.Set(safety_hub::kSafetyHubMenuNotificationImpressionCountKey,
               impression_count_);
  }
  if (first_impression_time_.has_value()) {
    result.Set(safety_hub::kSafetyHubMenuNotificationFirstImpressionKey,
               base::TimeToValue(first_impression_time_.value()));
  }
  if (last_impression_time_.has_value()) {
    result.Set(safety_hub::kSafetyHubMenuNotificationLastImpressionKey,
               base::TimeToValue(last_impression_time_.value()));
  }
  if (all_time_notification_count_ != 0) {
    result.Set(safety_hub::kSafetyHubMenuNotificationAllTimeCountKey,
               all_time_notification_count_);
  }
  if (show_only_after_.has_value()) {
    result.Set(safety_hub::kSafetyHubMenuNotificationShowAfterTimeKey,
               base::TimeToValue(show_only_after_.value()));
  }
  if (current_result_ != nullptr) {
    result.Set(safety_hub::kSafetyHubMenuNotificationResultKey,
               current_result_->ToDictValue());
  }
  return result;
}

void SafetyHubMenuNotification::Show() {
  ++impression_count_;
  if (!first_impression_time_.has_value()) {
    is_currently_active_ = true;
    should_be_shown_after_interval_ = false;
    first_impression_time_ = base::Time::Now();
  }
  last_impression_time_ = base::Time::Now();
}

void SafetyHubMenuNotification::Dismiss() {
  is_currently_active_ = false;
  impression_count_ = 0;
  first_impression_time_ = std::nullopt;
  ++all_time_notification_count_;
}

// The maximum all time impressions for a notification are passed on each call
// instead of determined in the constructor to make these easier
// Finch-configurable as the constructor will only called once initially for
// each Safety Hub module, after which it will always be read from disk.
bool SafetyHubMenuNotification::ShouldBeShown(
    base::TimeDelta interval,
    int max_all_time_impressions) const {
  // The type of notification has already been shown the maximum number of times
  // in the total lifetime.
  if (max_all_time_impressions > 0 &&
      all_time_notification_count_ >= max_all_time_impressions) {
    return false;
  }

  // There is no associated result, or the result does not meet the bar for menu
  // notifications.
  if (!current_result_ || !current_result_->IsTriggerForMenuNotification()) {
    return false;
  }

  // Do not show notification if it is too soon.
  if (show_only_after_.has_value() &&
      base::Time::Now() < show_only_after_.value()) {
    return false;
  }

  // Notifications that have never been shown can be shown as long as the result
  // is a trigger.
  if (!HasAnyNotificationBeenShown()) {
    return true;
  }

  // For active notifications, the notification should be shown if it is either
  // not shown enough times, or not sufficiently long enough.
  if (is_currently_active_) {
    return (!IsShownEnough());
  }

  // For notifications that are inactive, showing the notification is determined
  // by whether the interval has passed.
  return should_be_shown_after_interval_ && HasIntervalPassed(interval);
}

bool SafetyHubMenuNotification::IsCurrentlyActive() const {
  return is_currently_active_;
}

bool SafetyHubMenuNotification::IsShownEnough() const {
  // The notification has never been shown before.
  if (!first_impression_time_.has_value() ||
      !last_impression_time_.has_value()) {
    return false;
  }

  bool isShownEnoughDays =
      (base::Time::Now() - first_impression_time_.value()) >=
      kSafetyHubMenuNotificationMinNotificationDuration;

  bool isShownEnoughTimes =
      impression_count_ >= kSafetyHubMenuNotificationMinImpressionCount;

  return isShownEnoughDays && isShownEnoughTimes;
}

bool SafetyHubMenuNotification::HasIntervalPassed(
    base::TimeDelta interval) const {
  // Notification has never been shown, so no interval in this case.
  if (!HasAnyNotificationBeenShown()) {
    return true;
  }
  return base::Time::Now() - last_impression_time_.value() >= interval;
}

bool SafetyHubMenuNotification::HasAnyNotificationBeenShown() const {
  return last_impression_time_.has_value();
}

void SafetyHubMenuNotification::UpdateResult(
    std::unique_ptr<SafetyHubService::Result> new_result) {
  // Use the latest available result. This is either the current result when a
  // new result was received, or, if it is unavailble, the result that was
  // stored on the disk.
  base::Value::Dict previous_result_dict = current_result_
                                               ? current_result_->ToDictValue()
                                               : prev_stored_result_.Clone();
  // For notifications that are not currently active yet, and have a previous
  // result, we have to determine whether the notification should be shown after
  // the interval by comparing the old (stored) data with the new data to check
  // whether it warrants a new notification.
  if (!is_currently_active_ && !previous_result_dict.empty() &&
      new_result->WarrantsNewMenuNotification(previous_result_dict)) {
    should_be_shown_after_interval_ = true;
  }
  current_result_ = std::move(new_result);
}

std::u16string SafetyHubMenuNotification::GetNotificationString() const {
  CHECK(current_result_);
  return current_result_->GetNotificationString();
}

int SafetyHubMenuNotification::GetNotificationCommandId() const {
  CHECK(current_result_);
  return current_result_->GetNotificationCommandId();
}

SafetyHubService::Result* SafetyHubMenuNotification::GetResultForTesting()
    const {
  return current_result_.get();
}

void SafetyHubMenuNotification::SetOnlyShowAfter(base::Time time) {
  show_only_after_ = time;
}

void SafetyHubMenuNotification::ResetAllTimeNotificationCount() {
  all_time_notification_count_ = 0;
}

safety_hub::SafetyHubModuleType SafetyHubMenuNotification::GetModuleType()
    const {
  return module_type_;
}
