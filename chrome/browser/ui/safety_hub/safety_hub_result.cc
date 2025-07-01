// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_result.h"

#include "base/json/values_util.h"
#include "base/values.h"

SafetyHubResult::SafetyHubResult(base::Time timestamp)
    : timestamp_(timestamp) {}

base::Value::Dict SafetyHubResult::BaseToDictValue() const {
  base::Value::Dict result;
  result.Set(kSafetyHubTimestampResultKey, base::TimeToValue(timestamp_));
  return result;
}

base::Time SafetyHubResult::timestamp() const {
  return timestamp_;
}
