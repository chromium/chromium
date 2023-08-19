// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ANDROID_METRICS_HELPER_H_
#define COMPONENTS_METRICS_ANDROID_METRICS_HELPER_H_

#include <string>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace prefs {
constexpr char kVersionCodePref[] = "android_system_info.last_version_code";
}

// Whether 64-bit and/or 32-bit apps can be installed on this device/OS.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. See CpuAbiBitnessSupport in enums.xml.
enum class CpuAbiBitnessSupport {
  kNeither = 0,
  k32bitOnly = 1,
  k64bitOnly = 2,
  k32And64bit = 3,
  kMaxValue = k32And64bit,
};

// AndroidMetricsHelper is responsible for helping to log information related to
// system-level information about the Android device as well as the process.
class AndroidMetricsHelper {
 public:
  AndroidMetricsHelper(const AndroidMetricsHelper&) = delete;
  AndroidMetricsHelper& operator=(const AndroidMetricsHelper&) = delete;
  ~AndroidMetricsHelper() = default;

  static AndroidMetricsHelper* GetInstance();
  static AndroidMetricsHelper* CreateInstanceForTest(
      const std::string& version_code,
      bool has_abilist32,
      bool has_abilist64) {
    return new AndroidMetricsHelper(version_code, has_abilist32, has_abilist64);
  }

  int version_code_int() const { return version_code_int_; }
  CpuAbiBitnessSupport cpu_abi_bitness_support() const {
    return cpu_abi_bitness_support_;
  }

  // |on_did_create_metrics_log| denotes whether data is emitted in
  // OnDidCreateMetricsLog, as opposed to in ProvidePreviousSessionData.
  void EmitHistograms(PrefService* local_state, bool on_did_create_metrics_log);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Made public for testing.
  static void SaveLocalState(PrefService* local_state, int version_code_int);

  static void ResetGlobalStateForTesting() { local_state_saved_ = false; }

 private:
  friend struct AndroidMetricsHelperSingletonTraits;

  AndroidMetricsHelper(const std::string& version_code,
                       bool has_abilist32,
                       bool has_abilist64);

  int version_code_int_ = 0;
  CpuAbiBitnessSupport cpu_abi_bitness_support_ =
      CpuAbiBitnessSupport::kNeither;

  static inline bool local_state_saved_ = false;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_ANDROID_METRICS_HELPER_H_
