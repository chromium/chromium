// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification.h"

#include "base/json/values_util.h"

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
