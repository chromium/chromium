// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"

namespace device_signals {

namespace {

constexpr int kMaxSampleValue = 100;

constexpr char kUserPermissionHistogram[] =
    "Enterprise.DeviceSignals.UserPermission";
constexpr char kCollectionRequestHistogram[] =
    "Enterprise.DeviceSignals.Collection.Request";
constexpr char kCollectionSuccessHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success";
constexpr char kCollectionFailureHistogram[] =
    "Enterprise.DeviceSignals.Collection.Failure";

constexpr char kCollectionSuccessLatencyHistogramFormat[] =
    "Enterprise.DeviceSignals.Collection.Success.%s.Latency";
constexpr char kCollectionFailureLatencyHistogramFormat[] =
    "Enterprise.DeviceSignals.Collection.Failure.%s.Latency";
constexpr char kCollectionRequestItemsSizeHistogramFormat[] =
    "Enterprise.DeviceSignals.Collection.Request.%s.Items";
constexpr char kCollectionItemsSizeDeltaHistogramFormat[] =
    "Enterprise.DeviceSignals.Collection.%s.Delta";
constexpr char kCollectionSuccessSizeHistogramFormat[] =
    "Enterprise.DeviceSignals.Collection.Success.%s.Items";
constexpr char kCollectionSpecificFailureHistogramFormat[] =
    "Enterprise.DeviceSignals.Collection.Failure.%sLevelError";

std::string GetHistogramVariant(SignalName signal_name) {
  switch (signal_name) {
    case SignalName::kAntiVirus:
      return "AntiVirus";
    case SignalName::kHotfixes:
      return "Hotfixes";
    case SignalName::kFileSystemInfo:
      return "FileSystemInfo";
    case SignalName::kSystemSettings:
      return "SystemSettings";
    case SignalName::kAgent:
      return "Agent";
  }
}

std::string GetErrorHistogramVariant(SignalName signal_name,
                                     bool is_top_level_error) {
  return base::StringPrintf("%s.%s", GetHistogramVariant(signal_name).c_str(),
                            is_top_level_error ? "Top" : "Collection");
}

}  // namespace

void LogUserPermissionChecked(UserPermission permission) {
  base::UmaHistogramEnumeration(kUserPermissionHistogram, permission);
}

void LogSignalCollectionRequested(SignalName signal_name) {
  base::UmaHistogramEnumeration(kCollectionRequestHistogram, signal_name);
}

void LogSignalCollectionRequestedWithItems(SignalName signal_name,
                                           size_t number_of_items) {
  base::UmaHistogramExactLinear(
      base::StringPrintf(kCollectionRequestItemsSizeHistogramFormat,
                         GetHistogramVariant(signal_name).c_str()),
      number_of_items, kMaxSampleValue);
}

void LogSignalCollectionFailed(SignalName signal_name,
                               base::TimeTicks start_time,
                               SignalCollectionError error,
                               bool is_top_level_error) {
  base::UmaHistogramEnumeration(kCollectionFailureHistogram, signal_name);

  base::UmaHistogramTimes(
      base::StringPrintf(kCollectionFailureLatencyHistogramFormat,
                         GetHistogramVariant(signal_name).c_str()),
      base::TimeTicks::Now() - start_time);

  base::UmaHistogramEnumeration(
      base::StringPrintf(
          kCollectionSpecificFailureHistogramFormat,
          GetErrorHistogramVariant(signal_name, is_top_level_error).c_str()),
      error);
}

void LogSignalCollectionSucceeded(SignalName signal_name,
                                  base::TimeTicks start_time,
                                  std::optional<size_t> signal_collection_size,
                                  std::optional<size_t> signal_request_size) {
  base::UmaHistogramEnumeration(kCollectionSuccessHistogram, signal_name);

  const std::string histogram_variant = GetHistogramVariant(signal_name);
  base::UmaHistogramTimes(
      base::StringPrintf(kCollectionSuccessLatencyHistogramFormat,
                         histogram_variant.c_str()),
      base::TimeTicks::Now() - start_time);

  if (signal_collection_size.has_value()) {
    base::UmaHistogramExactLinear(
        base::StringPrintf(kCollectionSuccessSizeHistogramFormat,
                           histogram_variant.c_str()),
        signal_collection_size.value(), kMaxSampleValue);

    if (signal_request_size.has_value()) {
      base::UmaHistogramExactLinear(
          base::StringPrintf(kCollectionItemsSizeDeltaHistogramFormat,
                             histogram_variant.c_str()),
          signal_request_size.value() - signal_collection_size.value(),
          kMaxSampleValue);
    }
  }
}

void LogCrowdStrikeParsingError(SignalsParsingError error) {
  static constexpr char kCrowdStrikeErrorHistogram[] =
      "Enterprise.DeviceSignals.Collection.CrowdStrike.Error";
  base::UmaHistogramEnumeration(kCrowdStrikeErrorHistogram, error);
}

}  // namespace device_signals
