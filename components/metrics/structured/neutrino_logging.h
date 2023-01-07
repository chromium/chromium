// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_NEUTRINO_LOGGING_H_
#define COMPONENTS_METRICS_STRUCTURED_NEUTRINO_LOGGING_H_

#include <string>

// Some functions in this file are called by code in //components/metrics, so
// nothing here can depend upon //components/metrics. Functions that need to
// depend upon //components/metrics should be placed in neutrino_logging_util.h.

namespace metrics {
namespace structured {

// Specifies the location in the code that NeurinoDevices logging occurs.
// 1: Other
// 2-9: Code paths within ForceClientIdCreation.
// 10-99: Code paths leading to ForceClientIDCreation.
// 100-199: Code paths leading to UpdateMetricsPrefsOnPermissionChange.
// 200+: Other locations.
// These values are persisted to logs. Entries should not be renumbered and
// numerical values should never be reused.
enum class NeutrinoDevicesLocation {
  kOther = 1,
  kClientIdFromLocalState = 2,
  kClientIdBackupRecovered = 3,
  kClientIdNew = 4,
  kClientIdFromProvisionalId = 5,
  kEnableRecording = 10,
  kMetricsStateManager = 11,
  kCreateVariationsService = 12,
  kCreateMetricsServiceClient = 13,
  kCreateEntropyProvider = 14,
  kSetMetricsReporting = 101,
  kChangeMetricsReportingStateWithReply = 102,
  kOnEulaAccepted = 103,
  kFirstRunDiolag = 104,
  kMaybeEnableUma = 105,
  kNotifyObservers = 106,
  kIsEnabled = 107,
  kLoginDisplayHostWebUI = 201,
  kLoginDisplayHostWebUIDestructor = 202,
  kProvidePreviousSessionData = 203,
  kIsMetricsAndCrashReportingEnabled = 204,
  kIsMetricsReportingPolicyManaged = 205,
};

// Log the location in the code to the NeutrinoDevices structured metrics log.
void NeutrinoDevicesLog(const NeutrinoDevicesLocation location);

// Log the location in the code and the client id to the NeutrinoDevices
// structured metrics log.
void NeutrinoDevicesLogWithClientId(const std::string& client_id,
                                    NeutrinoDevicesLocation location);

// Log the policy status (managed or unmanaged) and location in the code
// to the NeutrinoDevices structured metrics log.
void NeutrinoDevicesLogPolicy(const std::string& client_id,
                              bool is_managed,
                              NeutrinoDevicesLocation location);

// Log that the client id has been cleared to the NeutrinoDevices structured
// metrics log.
void NeutrinoDevicesLogClientIdCleared(
    const std::string& client_id,
    int64_t install_date_timestamp,
    int64_t metrics_reporting_enabled_timestamp);

// Log that the client id has been changed to the NeutrinoDevices structured
// metrics log.
void NeutrinoDevicesLogClientIdChanged(
    const std::string& client_id,
    const std::string& initial_client_id,
    int64_t install_date_timestamp,
    int64_t metrics_reporting_enabled_timestamp,
    NeutrinoDevicesLocation location);

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_NEUTRINO_LOGGING_H_
