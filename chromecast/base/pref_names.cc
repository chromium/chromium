// Copyright 2014 The Chromium Authors. All rights reserved.
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

// Total number of child process crashes (other than renderer / extension
// renderer ones, and plugin children, which are counted separately) since the
// last report.
const char kStabilityChildProcessCrashCount[] =
    "user_experience_metrics.stability.child_process_crash_count";

// Total number of kernel crashes since the last report.
const char kStabilityKernelCrashCount[] =
    "user_experience_metrics.stability.kernel_crash_count";

// Total number of external user process crashes since the last report.
const char kStabilityOtherUserCrashCount[] =
    "user_experience_metrics.stability.other_user_crash_count";

// Number of times a renderer process crashed since the last report.
const char kStabilityRendererCrashCount[] =
    "user_experience_metrics.stability.renderer_crash_count";

// Number of times a renderer process failed to launch since the last report.
const char kStabilityRendererFailedLaunchCount[] =
    "user_experience_metrics.stability.renderer_failed_launch_count";

// Number of times the renderer has become non-responsive since the last
// report.
const char kStabilityRendererHangCount[] =
    "user_experience_metrics.stability.renderer_hang_count";

// Total number of unclean system shutdowns since the last report.
const char kStabilitySystemUncleanShutdownCount[] =
    "user_experience_metrics.stability.system_unclean_shutdowns";

// Current web color scheme. See blink::PreferredColorScheme.
const char kWebColorScheme[] = "web-color-scheme";

}  // namespace prefs
}  // namespace chromecast
