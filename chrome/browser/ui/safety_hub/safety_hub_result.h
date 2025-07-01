// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_RESULT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"

inline constexpr char kSafetyHubTimestampResultKey[] = "timestamp";
inline constexpr char kSafetyHubOriginKey[] = "origin";

// Base class for results returned after the periodic execution of a Safety
// Hub service. Each service should implement a derived class that captures
// the specific information that is retrieved. Any intermediate data that is
// required for the background task, or that needs to passed through to the UI
// thread task should be included as well.
class SafetyHubResult {
 public:
  virtual ~SafetyHubResult() = default;

  virtual base::Value::Dict ToDictValue() const = 0;

  // Determines whether the current result meets the bar for showing a
  // notification to the user in the Chrome menu.
  virtual bool IsTriggerForMenuNotification() const = 0;

  // Determines whether the previous result is sufficiently different that for
  // the current result a new notification should be shown. This indication is
  // just based on the comparison of the two results, and thus irrelevant to
  // how frequently a menu notification has already been shown.
  virtual bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const = 0;

  // Returns the string for the notification that will be shown in the
  // three-dot menu.
  virtual std::u16string GetNotificationString() const = 0;

  // Returns the command ID that should be run when the user clicks the
  // notification in the three-dot menu.
  virtual int GetNotificationCommandId() const = 0;

  // Returns a copy of the current Safety Hub object. This is intended to be
  // used when the caller is unaware of the specific derived class.
  virtual std::unique_ptr<SafetyHubResult> Clone() const = 0;

  base::Time timestamp() const;

 protected:
  explicit SafetyHubResult(base::Time timestamp = base::Time::Now());
  SafetyHubResult(const SafetyHubResult&) = default;
  SafetyHubResult& operator=(const SafetyHubResult&) = default;

  // Returns a dictionary representation of a base Result which consists of only
  // a timestamp.
  base::Value::Dict BaseToDictValue() const;

 private:
  base::Time timestamp_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_RESULT_H_
