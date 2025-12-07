// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/process_state.h"

#if !BUILDFLAG(IS_ANDROID)
#include <optional>
#endif  // !BUILDFLAG(IS_ANDROID)

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/common/chrome_features.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

bool g_is_incognito_process = false;

#if !BUILDFLAG(IS_ANDROID)
std::optional<bool>& GetIsInstantProcessMutable() {
  static std::optional<bool> is_instant_process;
  return is_instant_process;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

namespace process_state {

bool IsIncognitoProcess() {
  return g_is_incognito_process;
}

void SetIsIncognitoProcess(bool is_incognito_process) {
  g_is_incognito_process = is_incognito_process;
}

#if !BUILDFLAG(IS_ANDROID)
void SetIsInstantProcess(bool is_instant_process) {
  CHECK(base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer));
  GetIsInstantProcessMutable() = is_instant_process;
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool IsInstantProcess() {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer)) {
    std::optional<bool> is_instant_process = GetIsInstantProcessMutable();
    CHECK(is_instant_process.has_value());
    return is_instant_process.value();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kInstantProcess);
}

}  // namespace process_state
