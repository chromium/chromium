// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_log.h"

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

NavigationCapturingLog::NavigationCapturingLog() = default;
NavigationCapturingLog::~NavigationCapturingLog() = default;

void NavigationCapturingLog::StoreNavigationCapturedDebugData(
    base::Value value) {
  static const size_t kMaxLogLength =
      base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo) ? 1000
                                                                     : 20;

  debug_log_.push_front(std::move(value));
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
