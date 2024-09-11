// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_

#include <optional>
#include <unordered_set>
#include <vector>

#include "base/values.h"
#include "build/build_config.h"
#include "components/device_signals/core/common/common_types.h"

#if BUILDFLAG(IS_WIN)
#include "components/device_signals/core/common/win/win_types.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device_signals {

// Possible values for the trigger which generated the device signals.
enum class Trigger {
  kUnspecified = 0,
  kBrowserNavigation = 1,
  kLoginScreen = 2,
};

// Enum of names representing signals bundles that can be aggregated via the
// SignalsAggregator.
// These values are persisted to logs and should not be renumbered. Please
// update the DeviceSignalsSignalName enum in enums.xml when adding a new
// value here.
enum class SignalName {
  kAntiVirus,
  kHotfixes,
  kFileSystemInfo,
  kSystemSettings,
  kAgent,
  kMaxValue = kAgent
};

// Superset of all signal collection errors that can occur, including top-level
// as well as per-bundle errors.
// These values are persisted to logs and should not be renumbered. Please
// update the DeviceSignalsSignalCollectionError enum in enums.xml when adding a
// new value here.
enum class SignalCollectionError {
  kConsentRequired,
  kUnaffiliatedUser,
  kUnsupported,
  kMissingSystemService,
  kMissingBundle,
  kInvalidUser,
  kMissingParameters,
  kParsingFailed,
  kUnexpectedValue,
  kMaxValue = kUnexpectedValue
};

const std::string ErrorToString(SignalCollectionError error);

// Base struct type that each specific signal bundle types should extend. The
// derived signal bundles/responses should group a set of signals that
// cohesively belong together (e.g. device-level signals, policy values
// signals).
struct BaseSignalResponse {
  virtual ~BaseSignalResponse();

  // If set, represents a collection error that occurred while getting the
  // signal.
  std::optional<SignalCollectionError> collection_error = std::nullopt;
};

#if BUILDFLAG(IS_WIN)
struct AntiVirusSignalResponse : BaseSignalResponse {
  AntiVirusSignalResponse();

  AntiVirusSignalResponse(const AntiVirusSignalResponse&);
  AntiVirusSignalResponse& operator=(const AntiVirusSignalResponse&);

  ~AntiVirusSignalResponse() override;

  std::vector<AvProduct> av_products{};
};

struct HotfixSignalResponse : BaseSignalResponse {
  HotfixSignalResponse();

  HotfixSignalResponse(const HotfixSignalResponse&);
  HotfixSignalResponse& operator=(const HotfixSignalResponse&);

  ~HotfixSignalResponse() override;

  std::vector<InstalledHotfix> hotfixes{};
};
#endif  // BUILDFLAG(IS_WIN)

enum class RegistryHive {
  kHkeyClassesRoot,
  kHkeyLocalMachine,
  kHkeyCurrentUser,
  kMaxValue = kHkeyCurrentUser
};

struct GetSettingsOptions {
  GetSettingsOptions();

  GetSettingsOptions(const GetSettingsOptions&);
  GetSettingsOptions& operator=(const GetSettingsOptions&);

  ~GetSettingsOptions();

  // General path value usable by derived types.
  // On Windows it would be the path to the reg key inside the hive.
  // On Mac it would be the path to the plist file.
  std::string path{};

  // Key specifying the setting entry we're looking for.
  // On Windows, that will be the registry key itself.
  // On Mac, this is a key path used to retrieve a value from valueForKeyPath:.
  std::string key{};

  // When set to true, the retrieved signal will also include the setting’s
  // value. When false, the signal will only contain the setting’s
  // presence.
  // Supported types on Windows:
  // - REG_SZ
  // - REG_DWORD
  // - REG_QWORD
  // Supported types on Mac:
  // - NSString
  // - NSNumber
  bool get_value = false;

  // Windows registry hive containing the desired value. This values is required
  // on Windows, but will be ignored on Mac.
  std::optional<RegistryHive> hive = std::nullopt;

  bool operator==(const GetSettingsOptions& other) const;
};

struct SettingsItem {
  SettingsItem();

  SettingsItem(const SettingsItem&);
  SettingsItem& operator=(const SettingsItem&);

  ~SettingsItem();

  std::string path{};

  std::string key{};

  std::optional<RegistryHive> hive = std::nullopt;

  // Value indicating whether the specific resource could be found or not.
  PresenceValue presence = PresenceValue::kUnspecified;

  // JSON string representing the value of the setting. Only set when the
  // setting was found and `get_value` was true on the corresponding request
  // options.
  std::optional<std::string> setting_json_value = std::nullopt;

  bool operator==(const SettingsItem& other) const;
};

struct SettingsResponse : BaseSignalResponse {
  SettingsResponse();

  SettingsResponse(const SettingsResponse&);
  SettingsResponse& operator=(const SettingsResponse&);

  ~SettingsResponse() override;

  std::vector<SettingsItem> settings_items{};
};

struct FileSystemInfoResponse : BaseSignalResponse {
  FileSystemInfoResponse();

  FileSystemInfoResponse(const FileSystemInfoResponse&);
  FileSystemInfoResponse& operator=(const FileSystemInfoResponse&);

  ~FileSystemInfoResponse() override;

  std::vector<FileSystemItem> file_system_items{};
};

struct AgentSignalsResponse : BaseSignalResponse {
  AgentSignalsResponse();

  AgentSignalsResponse(const AgentSignalsResponse&);
  AgentSignalsResponse& operator=(const AgentSignalsResponse&);

  ~AgentSignalsResponse() override;

  std::optional<CrowdStrikeSignals> crowdstrike_signals = std::nullopt;
};

// Request struct containing properties that will be used by the
// SignalAggregator to validate signals access permissions while delegating
// the collection to the right Collectors. Signals that require parameters (e.g.
// FileSystemInfo) will look for them in this object.
struct SignalsAggregationRequest {
  SignalsAggregationRequest();

  SignalsAggregationRequest(const SignalsAggregationRequest&);
  SignalsAggregationRequest(SignalsAggregationRequest&&);
  SignalsAggregationRequest& operator=(const SignalsAggregationRequest&);
  SignalsAggregationRequest& operator=(SignalsAggregationRequest&&);

  ~SignalsAggregationRequest();

  // Names of the signals that need to be collected.
  std::unordered_set<SignalName> signal_names{};

  // Parameters required when requesting the collection of signals living on
  // the device's file system.
  std::vector<GetFileSystemInfoOptions> file_system_signal_parameters{};

  std::vector<GetSettingsOptions> settings_signal_parameters{};

  bool operator==(const SignalsAggregationRequest& other) const;
};

// Response from a signal collection request sent through the SignalsAggregator.
// The signal bundles on this object will be set according to the set of signal
// names given in the corresponding `SignalsAggregationRequest`.
struct SignalsAggregationResponse {
  SignalsAggregationResponse();

  SignalsAggregationResponse(const SignalsAggregationResponse&);
  SignalsAggregationResponse(SignalsAggregationResponse&&);
  SignalsAggregationResponse& operator=(const SignalsAggregationResponse&);
  SignalsAggregationResponse& operator=(SignalsAggregationResponse&&);

  ~SignalsAggregationResponse();

  // If set, represents an error that occurred before any signal could be
  // collected.
  std::optional<SignalCollectionError> top_level_error = std::nullopt;

#if BUILDFLAG(IS_WIN)
  std::optional<AntiVirusSignalResponse> av_signal_response = std::nullopt;
  std::optional<HotfixSignalResponse> hotfix_signal_response = std::nullopt;
#endif  // BUILDFLAG(IS_WIN)
  std::optional<SettingsResponse> settings_response = std::nullopt;

  std::optional<FileSystemInfoResponse> file_system_info_response =
      std::nullopt;

  std::optional<AgentSignalsResponse> agent_signals_response = std::nullopt;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_
