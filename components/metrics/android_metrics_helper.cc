// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_helper.h"

#include <set>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

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
  abi_bitness_support_ = has_abilist32
                             ? (has_abilist64 ? AbiBitnessSupport::k32And64bit
                                              : AbiBitnessSupport::k32bitOnly)
                             : (has_abilist64 ? AbiBitnessSupport::k64bitOnly
                                              : AbiBitnessSupport::kNeither);
  int output;
  if (base::StringToInt(version_code, &output)) {
    version_code_int_ = output;
  }
}

void AndroidMetricsHelper::EmitHistograms(bool current_session) const {
  if (version_code_int_) {
    // These may change across sessions, so log them only for current session.
    // Version code will sure change on every update. The other two are unlikely
    // to change, but some factors like RAM targeting may do it.
    //
    // TODO(crrev.com/c/1462131): Carry these across sessions using
    // SaveActivityTypeToLocalState/GetActivityTypeFromLocalState.
    if (current_session) {
      base::UmaHistogramSparse("Android.VersionCode", version_code_int_);
    }
  }

  base::UmaHistogramEnumeration("Android.AbiBitnessSupport",
                                abi_bitness_support_);
}

}  // namespace metrics
