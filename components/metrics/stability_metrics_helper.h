// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STABILITY_METRICS_HELPER_H_
#define COMPONENTS_METRICS_STABILITY_METRICS_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// The values here correspond to values in the Stability message in
// system_profile.proto.
// This must stay 1-1 with the StabilityEventType enum in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.metrics
enum class StabilityEventType {
  kPageLoad = 2,
  kRendererCrash = 3,
  // kRendererHang = 4,  // Removed due to disuse and correctness issues.
  kExtensionCrash = 5,
  // kChildProcessCrash = 6,  // Removed due to disuse and alternative metrics.
  kLaunch = 15,
  kBrowserCrash = 16,
  // kIncompleteShutdown = 17,  // Removed due to disuse and correctness issues.
  // kPluginCrash = 22,  // Removed due to plugin deprecation.
  kRendererFailedLaunch = 24,
  kExtensionRendererFailedLaunch = 25,
  kRendererLaunch = 26,
  kExtensionRendererLaunch = 27,
  kGpuCrash = 31,
  kUtilityCrash = 32,
  kUtilityLaunch = 33,
  kMaxValue = kUtilityLaunch,
};

class SystemProfileProto;

// Responsible for providing functionality common to different embedders'
// stability metrics providers.
class StabilityMetricsHelper {
 public:
  explicit StabilityMetricsHelper(PrefService* local_state);

  StabilityMetricsHelper(const StabilityMetricsHelper&) = delete;
  StabilityMetricsHelper& operator=(const StabilityMetricsHelper&) = delete;

  ~StabilityMetricsHelper();

#if BUILDFLAG(IS_ANDROID)
  // A couple Local-State-pref-based stability counts are retained for Android
  // WebView. Other platforms, including Android Chrome and WebLayer, should use
  // Stability.Counts2 as the source of truth for these counts.

  // Provides stability metrics.
  void ProvideStabilityMetrics(SystemProfileProto* system_profile_proto);

  // Clears the gathered stability metrics.
  void ClearSavedStabilityMetrics();
#endif  // BUILDFLAG(IS_ANDROID)

  // Records a utility process launch with name |metrics_name|.
  void BrowserUtilityProcessLaunched(const std::string& metrics_name);

  // Records a utility process crash with name |metrics_name|.
  void BrowserUtilityProcessCrashed(const std::string& metrics_name,
                                    int exit_code);

  // Records that a utility process with name |metrics_name| failed to launch.
  // The |launch_error_code| is a platform-specific error code. On Windows, a
  // |last_error| is also supplied to help diagnose the launch failure.
  void BrowserUtilityProcessLaunchFailed(const std::string& metrics_name,
                                         int launch_error_code
#if BUILDFLAG(IS_WIN)
                                         ,
                                         DWORD last_error
#endif
  );

  // Logs the initiation of a page load.
  void LogLoadStarted();

#if !BUILDFLAG(IS_ANDROID)
  // Records a renderer process crash.
  void LogRendererCrash(bool was_extension_process,
                        base::TerminationStatus status,
                        int exit_code);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Records that a new renderer process was successfully launched.
  void LogRendererLaunched(bool was_extension_process);

  // Records a renderer process hang.
  void LogRendererHang();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Increments the RendererCrash pref.
  void IncreaseRendererCrashCount();

  // Increments the GpuCrash pref.
  // Note: This is currently only used on Android. If you want to call this on
  // another platform, server-side processing code needs to be updated for that
  // platform to use the new data. Server-side currently assumes Android-only.
  void IncreaseGpuCrashCount();

  // Records a histogram for the input |stability_event_type|.
  static void RecordStabilityEvent(StabilityEventType stability_event_type);

 private:
  // Increments an Integer pref value specified by |path|.
  void IncrementPrefValue(const char* path);

  // Records that a renderer launch failed.
  void LogRendererLaunchFailed(bool was_extension_process);

  raw_ptr<PrefService> local_state_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_STABILITY_METRICS_HELPER_H_
