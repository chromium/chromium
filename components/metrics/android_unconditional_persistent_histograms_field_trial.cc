// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_unconditional_persistent_histograms_field_trial.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"

namespace metrics::android_unconditional_persistent_histograms_field_trial {
namespace {

const char kEnabledGroup[] = "Enabled";
const char kControlGroup[] = "Control";
const char kDefaultGroup[] = "Default";

const char* g_trial_group = nullptr;

}  // namespace

void EnrollClient() {
  // Prevent re-enrollment and additional dice rolls if this function is called
  // multiple times (e.g., in tests or due to multiple initialization paths).
  if (g_trial_group) {
    return;
  }

  int enabled_percent = 0;
  int control_percent = 0;
  // Stage 1: 50% Enabled on Canary/Dev, 100% Default on Beta/Stable.
  switch (version_info::android::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      enabled_percent = 100;
      control_percent = 0;
      break;
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      enabled_percent = 50;
      control_percent = 50;
      break;
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      enabled_percent = 0;
      control_percent = 0;
      break;
  }

  // Perform a standalone local dice roll without registering in the global
  // FieldTrialList.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrial::CreateSimulatedFieldTrial(
          kTrialName, 100, kDefaultGroup, base::RandDouble()));
  if (enabled_percent > 0) {
    trial->AppendGroup(kEnabledGroup, enabled_percent);
  }
  if (control_percent > 0) {
    trial->AppendGroup(kControlGroup, control_percent);
  }

  // Cache the assigned group name for later reporting as a synthetic field
  // trial.
  if (trial->group_name() == kEnabledGroup) {
    g_trial_group = kEnabledGroup;
  } else if (trial->group_name() == kControlGroup) {
    g_trial_group = kControlGroup;
  } else {
    g_trial_group = kDefaultGroup;
  }
}

bool IsEnabled() {
  return g_trial_group == kEnabledGroup;
}

std::string_view GetSyntheticTrialGroup() {
  if (g_trial_group) {
    return g_trial_group;
  }
  return std::string_view();
}

}  // namespace metrics::android_unconditional_persistent_histograms_field_trial
