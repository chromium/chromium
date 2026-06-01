// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ANDROID_UNCONDITIONAL_PERSISTENT_HISTOGRAMS_FIELD_TRIAL_H_
#define COMPONENTS_METRICS_ANDROID_UNCONDITIONAL_PERSISTENT_HISTOGRAMS_FIELD_TRIAL_H_

#include <string_view>

// Manages the local, simulated field trial for unconditionally enabling
// persistent histograms on Android early during startup.
//
// Because persistent histograms must be initialized before central field trials
// and Local State are available, this module performs a standalone local dice
// roll to assign an experimental group based on the Android channel. The group
// assignment is cached and reported later in startup as a synthetic field
// trial.
namespace metrics::android_unconditional_persistent_histograms_field_trial {

inline constexpr char kTrialName[] = "AndroidUnconditionalPersistentHistograms";

// Performs the local dice roll using a simulated field trial and caches the
// assigned group.
void EnrollClient();

// Returns true if the client is enrolled in the Enabled group, eliminating
// the spare file requirement.
bool IsEnabled();

// Returns the cached trial group name for reporting as a synthetic field trial.
std::string_view GetSyntheticTrialGroup();

}  // namespace metrics::android_unconditional_persistent_histograms_field_trial

#endif  // COMPONENTS_METRICS_ANDROID_UNCONDITIONAL_PERSISTENT_HISTOGRAMS_FIELD_TRIAL_H_
