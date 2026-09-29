// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/process_state.h"

#include <optional>

#include "base/command_line.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"

namespace {

bool g_is_incognito_process = false;

std::optional<bool>& GetIsInstantProcessMutable() {
  static std::optional<bool> is_instant_process;
  return is_instant_process;
}

}  // namespace

namespace process_state {

bool IsIncognitoProcess() {
  return g_is_incognito_process;
}

void SetIsIncognitoProcess(bool is_incognito_process) {
  g_is_incognito_process = is_incognito_process;
}

void SetIsInstantProcess(bool is_instant_process) {
  CHECK(base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer));
  GetIsInstantProcessMutable() = is_instant_process;
}

bool IsInstantProcess() {
  if (base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer)) {
    std::optional<bool> is_instant_process = GetIsInstantProcessMutable();
    CHECK(is_instant_process.has_value());
    return is_instant_process.value();
  }
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kInstantProcess);
}

}  // namespace process_state
