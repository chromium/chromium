// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_helper.h"

#include <set>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include <sys/system_properties.h>
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace metrics {

// static
AndroidMetricsHelper* AndroidMetricsHelper::GetInstance() {
#if BUILDFLAG(IS_ANDROID)
  char abilist32[PROP_VALUE_MAX];
  char abilist64[PROP_VALUE_MAX];
  static AndroidMetricsHelper instance(
      base::android::BuildInfo::GetInstance()->package_version_code(),
      __system_property_get("ro.product.cpu.abilist32", abilist32) > 0,
      __system_property_get("ro.product.cpu.abilist64", abilist64) > 0);
#else
  static AndroidMetricsHelper instance("", false, false);
#endif
  return &instance;
}

AndroidMetricsHelper::AndroidMetricsHelper(const std::string& version_code,
                                           bool has_abilist32,
                                           bool has_abilist64) {
  cpu_abi_bitness_support_ =
      has_abilist32 ? (has_abilist64 ? CpuAbiBitnessSupport::k32And64bit
                                     : CpuAbiBitnessSupport::k32bitOnly)
                    : (has_abilist64 ? CpuAbiBitnessSupport::k64bitOnly
                                     : CpuAbiBitnessSupport::kNeither);
  int output;
  if (base::StringToInt(version_code, &output)) {
    version_code_int_ = output;
  }
}

void AndroidMetricsHelper::EmitHistograms(PrefService* local_state,
                                          bool on_did_create_metrics_log) {
  if (on_did_create_metrics_log) {
    if (version_code_int_) {
      // The values won't change within the session, so save only once.
      if (!local_state_saved_) {
        // version_code_int_ can change across session. Save it so that it can
        // be restored in case the session dies before logs are flushed.
        // cpu_abi_bitness_support_ doesn't change across sessions (that'd
        // require OS reinstall), so no need to save it. It can be reliably
        // reconstructed in the next session.
        SaveLocalState(local_state, version_code_int_);
        local_state_saved_ = true;
      }

      // This may change across sessions, so log it only for current session.
      base::UmaHistogramSparse("Android.VersionCode", version_code_int_);
    }
  } else {
    // Make sure we didn't overwrite the stored state yet.
    CHECK(!local_state_saved_);
    // For previous session, don't log version_code_int_ as version code may
    // have changed (e.g. version update). Log saved value instead.
    int restored_version_code_int =
        local_state->GetInteger(prefs::kVersionCodePref);
    if (restored_version_code_int) {
      base::UmaHistogramSparse("Android.VersionCode",
                               restored_version_code_int);
    }
  }

  // cpu_abi_bitness_support_ doesn't change across sessions (that'd require OS
  // reinstall), so no need to load it, just use the current value.
  base::UmaHistogramEnumeration("Android.CpuAbiBitnessSupport",
                                cpu_abi_bitness_support_);
}

// static
void AndroidMetricsHelper::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kVersionCodePref, 0);
  local_state_saved_ = false;
}

// static
void AndroidMetricsHelper::SaveLocalState(PrefService* local_state,
                                          int version_code_int) {
  local_state->SetInteger(prefs::kVersionCodePref, version_code_int);
}

}  // namespace metrics
