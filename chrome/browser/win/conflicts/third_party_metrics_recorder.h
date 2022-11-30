// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_H_

#include <string>

#include "build/branding_buildflags.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/timer/timer.h"
#endif

struct ModuleInfoData;
struct ModuleInfoKey;

// Records metrics about third party modules loaded into Chrome.
class ThirdPartyMetricsRecorder : public ModuleDatabaseObserver {
 public:
  ThirdPartyMetricsRecorder();

  ThirdPartyMetricsRecorder(const ThirdPartyMetricsRecorder&) = delete;
  ThirdPartyMetricsRecorder& operator=(const ThirdPartyMetricsRecorder&) =
      delete;

  ~ThirdPartyMetricsRecorder() override;

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetHookDisabled() { hook_enabled_ = false; }
#endif

 private:
  // The size of the unsigned modules crash keys.
  static constexpr size_t kCrashKeySize = 256;

  // A helper function that writes the unsigned modules name to a series of
  // crash keys. The crash keys are leaked so that they can be picked up by the
  // crash reporter. Creating another instance of ThirdPartyMetricsRecorder
  // will start overwriting the current values in the crash keys. This is not
  // a problem in practice because this class is leaked.
  void AddUnsignedModuleToCrashkeys(const std::wstring& module_basename);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Invoked periodically to record heartbeat metrics related to third-party
  // DLL blocking.
  void RecordHeartbeatMetrics();

  // Timer that controls when heartbeat metrics are recorded.
  base::RepeatingTimer heartbeat_metrics_timer_;

  // Indicates if the ThirdPartyModules.Heartbeat.BlockedModulesCount heartbeat
  // metric is being recorded.
  bool record_blocked_modules_count_ = true;

  // Indicates if the blocking of third-party DLLs is still enabled or if it
  // was disabled because in-process printing was invoked.
  // See ModuleDatabase::DisableThirdPartyBlocking().
  bool hook_enabled_ = true;
#endif

  // The index of the crash key that is currently being updated.
  size_t current_key_index_ = 0;

  // The value of the crash key that is currently being updated.
  std::string current_value_;

  // Flag used to avoid sending module counts multiple times.
  bool metrics_emitted_ = false;

  // Counters for different types of modules.
  size_t module_count_ = 0;
  size_t unsigned_module_count_ = 0;
  size_t signed_module_count_ = 0;
  size_t catalog_module_count_ = 0;
  size_t microsoft_module_count_ = 0;
  size_t loaded_third_party_module_count_ = 0;
  size_t not_loaded_third_party_module_count_ = 0;

  // Counts the number of shell extensions.
  size_t shell_extensions_count_ = 0;
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_H_
