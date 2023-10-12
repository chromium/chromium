// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/gms_metrics_provider.h"

#include "base/logging.h"

#include "base/android/build_info.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"

namespace metrics {
namespace {

enum class GmsShortVersionCode {
  kNotInstalled = 0,
  kNotParsable = 1,
  kOutOfRange = 2,
  kMaxValue = kOutOfRange,
};

// Minimum valid GMS Core version.
constexpr int kMinimumVersion = 200302000;

// Maximum valid GMS Core version.
constexpr int kMaxValidVersion = 301200000;

void RecordGMSCoreVersionCode(int version) {
  base::UmaHistogramSparse("Android.PlayServices.ShortVersion", version);
}

}  // namespace

GmsMetricsProvider::GmsMetricsProvider() = default;
GmsMetricsProvider::~GmsMetricsProvider() = default;

bool GmsMetricsProvider::ProvideHistograms() {
  int current_gms_core_version;
  if (!base::StringToInt(GetGMSVersion(), &current_gms_core_version)) {
    RecordGMSCoreVersionCode(
        static_cast<int>(GmsShortVersionCode::kNotParsable));
    return true;
  }

  if (current_gms_core_version == 0) {
    RecordGMSCoreVersionCode(
        static_cast<int>(GmsShortVersionCode::kNotInstalled));
    return true;
  }

  // Get rid of old versions and garbage.
  if (current_gms_core_version < kMinimumVersion ||
      current_gms_core_version > kMaxValidVersion) {
    RecordGMSCoreVersionCode(
        static_cast<int>(GmsShortVersionCode::kOutOfRange));
    return true;
  }
  // Get first four digits representing a year and a week.
  int year_weak_code = current_gms_core_version / 100000;
  // Get following two digits indicating build version, version greater or equal
  // to 12 are stable releases.
  bool is_stable_release = ((current_gms_core_version / 1000) % 100) >= 12;

  // Log the current version in a YYWWV format, where 24031 would indicate
  // 2024y03w stable release.
  RecordGMSCoreVersionCode(year_weak_code * 10 + is_stable_release);
  return true;
}

std::string GmsMetricsProvider::GetGMSVersion() {
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
  return info->gms_version_code();
}

}  // namespace metrics