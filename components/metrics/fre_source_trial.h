// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_FRE_SOURCE_TRIAL_H_
#define COMPONENTS_METRICS_FRE_SOURCE_TRIAL_H_

#include "base/metrics/field_trial.h"
#include "base/version_info/channel.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics::fre_source_trial {

// Trial and group names for the FileMetricsProvider FRE trial.
inline constexpr char kFRESourceTrial[] = "FileMetricsProviderFRESourceTrial";
inline constexpr char kDefaultGroup[] = "Default";
inline constexpr char kControlGroup[] = "Control";
inline constexpr char kEnabledGroup[] = "Enabled";

// Registers the local state prefs for the FileMetricsProvider FRE trial.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Creates the FileMetricsProvider FRE trial.
void Create(PrefService* local_state,
            const base::FieldTrial::EntropyProvider& entropy_provider,
            version_info::Channel channel,
            bool is_fre);

// Returns true if the FileMetricsProvider FRE trial is enabled.
bool IsEnabled();

}  // namespace metrics::fre_source_trial

#endif  // COMPONENTS_METRICS_FRE_SOURCE_TRIAL_H_
