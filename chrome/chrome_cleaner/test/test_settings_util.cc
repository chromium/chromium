// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_settings_util.h"

namespace chrome_cleaner {

SettingsWithExecutionModeOverride::SettingsWithExecutionModeOverride(
    ExecutionMode execution_mode)
    : execution_mode_(execution_mode) {}

SettingsWithExecutionModeOverride::~SettingsWithExecutionModeOverride() =
    default;

ExecutionMode SettingsWithExecutionModeOverride::execution_mode() const {
  return execution_mode_;
}

MockSettings::MockSettings() = default;

MockSettings::~MockSettings() = default;

}  // namespace chrome_cleaner
