// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_log.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

NavigationCapturingLog::NavigationCapturingLog() = default;
NavigationCapturingLog::~NavigationCapturingLog() = default;

void NavigationCapturingLog::LogData(
    std::string_view source,
    base::Value value,
    std::optional<int64_t> navigation_handle_id) {
  static const size_t kMaxLogLength =
      base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo) ? 1000
                                                                     : 20;
  base::Value::Dict log_entry;
  log_entry.Set("source", source);
  log_entry.Set("navigation_id",
                base::saturated_cast<int>(navigation_handle_id.value_or(-1)));
  log_entry.Set("value", std::move(value));

  DVLOG(1) << log_entry.DebugString();
  debug_log_.emplace_front(std::move(log_entry));
  if (debug_log_.size() > kMaxLogLength) {
    debug_log_.resize(kMaxLogLength);
  }
}

base::Value NavigationCapturingLog::GetLog() const {
  base::Value::List log;
  for (const auto& command_value : debug_log_) {
    log.Append(command_value.Clone());
  }
  return base::Value(std::move(log));
}

}  // namespace web_app
