// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/pref_names.h"

namespace chromecast {
namespace prefs {

// List of experiments enabled by DCS. For metrics purposes only.
const char kActiveDCSExperiments[] = "experiments.ids";

// Dictionary of remotely-enabled features from the latest DCS config fetch.
const char kLatestDCSFeatures[] = "experiments.features";

// Boolean that specifies whether or not the client_id has been regenerated
// due to bug b/9487011.
const char kMetricsIsNewClientID[] = "user_experience_metrics.is_new_client_id";

// Whether or not to report metrics and crashes.
const char kOptInStats[] = "opt-in.stats";

// Whether or not TOS has been accepted by user.
const char kTosAccepted[] = "tos-accepted";

// Total number of kernel crashes since the last report.
const char kStabilityKernelCrashCount[] =
    "user_experience_metrics.stability.kernel_crash_count";

// Total number of external user process crashes since the last report.
const char kStabilityOtherUserCrashCount[] =
    "user_experience_metrics.stability.other_user_crash_count";

// Total number of unclean system shutdowns since the last report.
const char kStabilitySystemUncleanShutdownCount[] =
    "user_experience_metrics.stability.system_unclean_shutdowns";

// Current web color scheme. See blink::PreferredColorScheme.
const char kWebColorScheme[] = "web-color-scheme";

}  // namespace prefs
}  // namespace chromecast
