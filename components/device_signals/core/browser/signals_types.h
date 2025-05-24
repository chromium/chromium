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
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(IS_WIN)
#include "components/device_signals/core/common/win/win_types.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device_signals {

// Possible values for the trigger which generated the device signals.
enum class Trigger {
  kUnspecified = 0,
  kBrowserNavigation = 1,  // Ash only
  kLoginScreen = 2,        // Ash only
  kSignalsReport = 3,
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
  kOsSignals,
  kBrowserContextSignals,
  kMaxValue = kBrowserContextSignals
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

  bool operator==(const BaseSignalResponse&) const;

  // If set, represents a collection error that occurred while getting the
  // signal.
  std::optional<SignalCollectionError> collection_error = std::nullopt;
};

#if BUILDFLAG(IS_WIN)
// Values representing the overall antivirus software state of a device.
enum class InstalledAntivirusState {
  kNone = 0,
  kDisabled = 1,
  kEnabled = 2,
};

struct AntiVirusSignalResponse : BaseSignalResponse {
  AntiVirusSignalResponse();

  AntiVirusSignalResponse(const AntiVirusSignalResponse&);
  AntiVirusSignalResponse& operator=(const AntiVirusSignalResponse&);

  bool operator==(const AntiVirusSignalResponse&) const;

  ~AntiVirusSignalResponse() override;

  std::vector<AvProduct> av_products{};

  InstalledAntivirusState antivirus_state{InstalledAntivirusState::kNone};
};

struct HotfixSignalResponse : BaseSignalResponse {
  HotfixSignalResponse();

  HotfixSignalResponse(const HotfixSignalResponse&);
  HotfixSignalResponse& operator=(const HotfixSignalResponse&);

  bool operator==(const HotfixSignalResponse&) const;

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

  bool operator==(const GetSettingsOptions&) const;

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
};

struct SettingsItem {
  SettingsItem();

  SettingsItem(const SettingsItem&);
  SettingsItem& operator=(const SettingsItem&);

  bool operator==(const SettingsItem&) const;

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
};

struct SettingsResponse : BaseSignalResponse {
  SettingsResponse();

  SettingsResponse(const SettingsResponse&);
  SettingsResponse& operator=(const SettingsResponse&);

  bool operator==(const SettingsResponse&) const;

  ~SettingsResponse() override;

  std::vector<SettingsItem> settings_items{};
};

struct OsSignalsResponse : BaseSignalResponse {
  OsSignalsResponse();

  OsSignalsResponse(const OsSignalsResponse&);
  OsSignalsResponse& operator=(const OsSignalsResponse&);

  bool operator==(const OsSignalsResponse&) const;

  ~OsSignalsResponse() override;

  // Common to all platforms
  std::optional<std::string> display_name = std::nullopt;
  std::string browser_version{};
  std::optional<std::string> device_enrollment_domain = std::nullopt;
  std::string device_manufacturer{};
  std::string device_model{};
  device_signals::SettingValue disk_encryption =
      device_signals::SettingValue::UNKNOWN;
  std::optional<std::string> hostname = std::nullopt;
  std::optional<std::vector<std::string>> mac_addresses = std::nullopt;
  std::string operating_system{};
  device_signals::SettingValue os_firewall =
      device_signals::SettingValue::UNKNOWN;
  std::string os_version{};
  device_signals::SettingValue screen_lock_secured =
      device_signals::SettingValue::UNKNOWN;
  std::optional<std::string> serial_number = std::nullopt;
  std::optional<std::vector<std::string>> system_dns_servers = std::nullopt;

  // Windows specific
  std::optional<std::string> machine_guid = std::nullopt;
  std::optional<device_signals::SettingValue> secure_boot_mode = std::nullopt;
  std::optional<std::string> windows_machine_domain = std::nullopt;
  std::optional<std::string> windows_user_domain = std::nullopt;
};

struct ProfileSignalsResponse : BaseSignalResponse {
  ProfileSignalsResponse();

  ProfileSignalsResponse(const ProfileSignalsResponse&);
  ProfileSignalsResponse& operator=(const ProfileSignalsResponse&);

  bool operator==(const ProfileSignalsResponse&) const;

  ~ProfileSignalsResponse() override;

  bool built_in_dns_client_enabled;
  bool chrome_remote_desktop_app_blocked;
  std::optional<safe_browsing::PasswordProtectionTrigger>
      password_protection_warning_trigger = std::nullopt;
  std::optional<std::string> profile_enrollment_domain = std::nullopt;
  safe_browsing::SafeBrowsingState safe_browsing_protection_level;
  bool site_isolation_enabled;

  // Enterprise cloud content analysis exclusives
  enterprise_connectors::EnterpriseRealTimeUrlCheckMode realtime_url_check_mode;
  std::vector<std::string> file_downloaded_providers{};
  std::vector<std::string> file_attached_providers{};
  std::vector<std::string> bulk_data_entry_providers{};
  std::vector<std::string> print_providers{};
  std::vector<std::string> security_event_providers{};
};

struct FileSystemInfoResponse : BaseSignalResponse {
  FileSystemInfoResponse();

  FileSystemInfoResponse(const FileSystemInfoResponse&);
  FileSystemInfoResponse& operator=(const FileSystemInfoResponse&);

  bool operator==(const FileSystemInfoResponse&) const;

  ~FileSystemInfoResponse() override;

  std::vector<FileSystemItem> file_system_items{};
};

struct AgentSignalsResponse : BaseSignalResponse {
  AgentSignalsResponse();

  AgentSignalsResponse(const AgentSignalsResponse&);
  AgentSignalsResponse& operator=(const AgentSignalsResponse&);

  bool operator==(const AgentSignalsResponse&) const;

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

  bool operator==(const SignalsAggregationRequest&) const;

  ~SignalsAggregationRequest();

  // Names of the signals that need to be collected.
  std::unordered_set<SignalName> signal_names{};

  // Parameters required when requesting the collection of signals living on
  // the device's file system.
  std::vector<GetFileSystemInfoOptions> file_system_signal_parameters;

  std::vector<GetSettingsOptions> settings_signal_parameters;

  // Trigger source of the report, for non-ash platforms the default is
  // `kUnspecified`.
  Trigger trigger = Trigger::kUnspecified;
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

  bool operator==(const SignalsAggregationResponse&) const;

  ~SignalsAggregationResponse();

  // If set, represents an error that occurred before any signal could be
  // collected.
  std::optional<SignalCollectionError> top_level_error = std::nullopt;

#if BUILDFLAG(IS_WIN)
  std::optional<AntiVirusSignalResponse> av_signal_response = std::nullopt;
  std::optional<HotfixSignalResponse> hotfix_signal_response = std::nullopt;
#endif  // BUILDFLAG(IS_WIN)
  std::optional<SettingsResponse> settings_response = std::nullopt;
  std::optional<OsSignalsResponse> os_signals_response = std::nullopt;
  std::optional<ProfileSignalsResponse> profile_signals_response = std::nullopt;

  std::optional<FileSystemInfoResponse> file_system_info_response =
      std::nullopt;

  std::optional<AgentSignalsResponse> agent_signals_response = std::nullopt;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_
