// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification.h"

#include <memory>
#include <string>

#include "base/json/values_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

SafetyHubMenuNotification::SafetyHubMenuNotification()
    : is_currently_active_(false),
      impression_count_(0),
      first_impression_time_(absl::nullopt),
      last_impression_time_(absl::nullopt) {}

SafetyHubMenuNotification::~SafetyHubMenuNotification() = default;

SafetyHubMenuNotification::SafetyHubMenuNotification(
    const base::Value::Dict& dict) {
  is_currently_active_ =
      dict.FindBool(kSafetyHubMenuNotificationActiveKey).value();
  impression_count_ =
      dict.FindInt(kSafetyHubMenuNotificationImpressionCountKey).value();
  first_impression_time_ = base::ValueToTime(
      dict.Find(kSafetyHubMenuNotificationFirstImpressionKey));
  last_impression_time_ =
      base::ValueToTime(dict.Find(kSafetyHubMenuNotificationLastImpressionKey));
}

base::Value::Dict SafetyHubMenuNotification::ToDictValue() const {
  base::Value::Dict result;
  result.Set(kSafetyHubMenuNotificationActiveKey, is_currently_active_);
  result.Set(kSafetyHubMenuNotificationImpressionCountKey, impression_count_);
  if (first_impression_time_.has_value()) {
    result.Set(kSafetyHubMenuNotificationFirstImpressionKey,
               base::TimeToValue(first_impression_time_.value()));
  }
  if (last_impression_time_.has_value()) {
    result.Set(kSafetyHubMenuNotificationLastImpressionKey,
               base::TimeToValue(last_impression_time_.value()));
  }
  if (result_ != nullptr) {
    result.Set(kSafetyHubMenuNotificationResultKey, result_->ToDictValue());
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
  first_impression_time_ = absl::nullopt;
  // TODO(crbug.com/1443466): Capture lifetime count, and determine whether it
  // should still be shown. E.g. SafeBrowsing notification should only be shown
  // 3 times in total.
}

bool SafetyHubMenuNotification::ShouldBeShown(base::TimeDelta interval) const {
  // There is no associated result, or the result does not meet the bar for menu
  // notifications.
  if (!result_ || !result_->IsTriggerForMenuNotification()) {
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
      (base::Time::Now() - first_impression_time_.value()) >
      kSafetyHubMenuNotificationMinNotificationDuration;

  bool isShownEnoughTimes =
      impression_count_ > kSafetyHubMenuNotificationMinImpressionCount;

  return isShownEnoughDays && isShownEnoughTimes;
}

bool SafetyHubMenuNotification::HasIntervalPassed(
    base::TimeDelta interval) const {
  // Notification has never been shown, so no interval in this case.
  if (!HasAnyNotificationBeenShown()) {
    return true;
  }
  return base::Time::Now() - last_impression_time_.value() > interval;
}

bool SafetyHubMenuNotification::HasAnyNotificationBeenShown() const {
  return last_impression_time_.has_value();
}

void SafetyHubMenuNotification::UpdateResult(
    std::unique_ptr<SafetyHubService::Result> result) {
  if (!is_currently_active_ && result_ &&
      result_->WarrantsNewMenuNotification(*result.get())) {
    should_be_shown_after_interval_ = true;
  }
  result_ = std::move(result);
}

// static
std::unique_ptr<SafetyHubMenuNotification>
SafetyHubMenuNotification::FromDictValue(const base::Value::Dict& dict,
                                         SafetyHubService* service) {
  auto notification = std::make_unique<SafetyHubMenuNotification>(dict);
  if (dict.contains(kSafetyHubMenuNotificationResultKey)) {
    notification->result_ = service->GetResultFromDictValue(
        *dict.FindDict(kSafetyHubMenuNotificationResultKey));
  }
  return notification;
}

std::u16string SafetyHubMenuNotification::GetNotificationString() const {
  CHECK(result_);
  return result_->GetNotificationString();
}

int SafetyHubMenuNotification::GetNotificationCommandId() const {
  CHECK(result_);
  return result_->GetNotificationCommandId();
}
