// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains helper functions which provide information about the
// current version of Chrome. This includes channel information, version
// information etc. This functionality is provided by using functions in
// kernel32 and advapi32. No other dependencies are allowed in this file.

#ifndef CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
#define CHROME_INSTALL_STATIC_INSTALL_UTIL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/win/windows_types.h"

namespace version_info {
enum class Channel;
}

namespace install_static {

enum class ChannelOrigin;
struct InstallConstants;

// Registry key to store the stats/crash sampling state of Chrome. If set to 1,
// stats and crash reports will be uploaded in line with the user's consent,
// otherwise, uploads will be disabled. It is used to sample clients, to reduce
// server load for metics and crashes. This is controlled by the
// MetricsReporting feature in chrome_metrics_services_manager_client.cc and is
// written when metrics services are started up and when consent changes.
extern const wchar_t kRegValueChromeStatsSample[];

// TODO(ananta)
// https://crbug.com/604923
// Unify these constants with env_vars.h.
extern const wchar_t kHeadless[];
extern const wchar_t kShowRestart[];
extern const wchar_t kRestartInfo[];
extern const wchar_t kRtlLocale[];

// TODO(ananta)
// https://crbug.com/604923
// Unify these constants with those defined in content_switches.h.
extern const wchar_t kCrashpadHandler[];
extern const wchar_t kFallbackHandler[];
extern const wchar_t kProcessType[];
extern const wchar_t kUserDataDirSwitch[];
extern const wchar_t kUtilityProcess[];

// Used for suppressing warnings.
template <typename T>
inline void IgnoreUnused(T) {}

// Returns true if Chrome is running at system level.
bool IsSystemInstall();

// Returns the string "[kCompanyPathName\]kProductPathName[install_suffix]"
std::wstring GetChromeInstallSubDirectory();

// Returns the path
// "Software\[kCompanyPathName\]kProductPathName[install_suffix]". This subkey
// of HKEY_CURRENT_USER can be used to save and restore state. With the
// exception of data that is used by third parties (e.g., a subkey that
// specifies the location of a native messaging host's manifest), state stored
// in this key is removed during uninstall when the user chooses to also delete
// their browsing data.
std::wstring GetRegistryPath();

// The next set of registry paths are generally for integration with an Omaha
// updater (see https://github.com/google/omaha); Google Chrome builds integrate
// with Google Update. For all accesses, HKLM or HKCU must be used based on
// IsSystemInstall(). Additionally, KEY_WOW64_32KEY must be used for all
// accesses, as Omaha updaters exclusively use the 32-bit view of the registry.

// Returns the path "Software\Google\Update\Clients\<guid>" where "<guid>" is
// the current install mode's appguid. This key is primarily used for
// registering the browser as an app managed by the updater.
std::wstring GetClientsKeyPath();

// Returns the path "Software\Google\Update\ClientState\<guid>" where "<guid>"
// is the current install mode's appguid. This key is primarily (but not
// exclusively) used for holding install-wide state that is used by both the
// updater and the browser.
std::wstring GetClientStateKeyPath();

// Returns the path "Software\Google\Update\ClientStateMedium\<guid>" where
// "<guid>" is the current install mode's appguid. This is is used exclusively
// for system-wide installs to hold values written by the browser.
std::wstring GetClientStateMediumKeyPath();

// Returns the path
// "Software\Microsoft\Windows\CurrentVersion\Uninstall\[kCompanyPathName ]
// kProductPathName[install_suffix]. This is the key used for the browser's
// "Programs and Features" control panel entry for non-MSI installs (the entry
// for MSI installs is created and owned by Windows Installer).
std::wstring GetUninstallRegistryPath();

// Returns the app GUID with which Chrome is registered with Google Update, or
// an empty string if this brand does not integrate with Google Update. This is
// a simple convenience wrapper around InstallDetails.
const wchar_t* GetAppGuid();

// Returns the toast activator CLSID with which Chrome is registered with the
// the Windows OS.
const CLSID& GetToastActivatorClsid();

// Returns the Elevation Service CLSID, IID, Name, and Display Name
// respectively.
const CLSID& GetElevatorClsid();
const IID& GetElevatorIid();
std::wstring GetElevationServiceName();
std::wstring GetElevationServiceDisplayName();

// Returns the Tracing Service CLSID, IID, Name, and Display Name respectively.
const CLSID& GetTracingServiceClsid();
const IID& GetTracingServiceIid();
std::wstring GetTracingServiceName();
std::wstring GetTracingServiceDisplayName();

// Returns the unsuffixed application name of this program. This is the base of
// the name registered with Default Programs. IMPORTANT: This must only be
// called by the installer.
std::wstring GetBaseAppName();

// Returns the unsuffixed portion of the AppUserModelId. The AppUserModelId is
// used to group an app's windows together on the Windows taskbar along with its
// corresponding shortcuts; see
// https://msdn.microsoft.com/library/windows/desktop/dd378459.aspx for more
// information. Use ShellUtil::GetBrowserModelId to get the suffixed value -- it
// is almost never correct to use the unsuffixed (base) portion of this id
// directly.
const wchar_t* GetBaseAppId();

// Returns the browser's ProgID prefix (e.g., ChromeHTML or ChromiumHTM). The
// full id is of the form |prefix|.|suffix| and is limited to a maximum length
// of 39 characters including null-terminator; see
// https://msdn.microsoft.com/library/windows/desktop/dd542719.aspx for details.
// We define |suffix| as a fixed-length 26-character alphanumeric identifier,
// therefore the return value of this function must have a maximum length of
// 39 - 1(null-term) - 26(|suffix|) - 1(dot separator) = 11 characters.
const wchar_t* GetBrowserProgIdPrefix();

// Returns the browser's ProgId description.
const wchar_t* GetBrowserProgIdDescription();

// Returns the browser's PDF viewer ProgID prefix (e.g., ChromePDF or
// ChromiumPDF). See GetBrowserProgIdPrefix() comments for ProgID constraints.
const wchar_t* GetPDFProgIdPrefix();

// Returns the PDF document ProgId description.
const wchar_t* GetPDFProgIdDescription();

// Returns the path to the Active Setup registry entries
// (e.g., Software\Microsoft\Active Setup\Installed Components\[guid]).
std::wstring GetActiveSetupPath();

// Returns the legacy CommandExecuteImpl CLSID, or an empty string if the
// install mode never included a DelegateExecute verb handler.
std::wstring GetLegacyCommandExecuteImplClsid();

// Returns true if this mode supports in-product mechanisms to make the browser
// the user's chosen default browser.
bool SupportsSetAsDefaultBrowser();

// Returns the index of the app icon resource in the main executable for the
// mode.
int GetAppIconResourceIndex();

// Returns the index of the HTML icon resource for .html and related files.
int GetHTMLIconResourceIndex();

// Returns the index of the PDF icon resource for pdf files.
int GetPDFIconResourceIndex();

// Get sandbox id of current install mode.
const wchar_t* GetSandboxSidPrefix();

// Returns the brand-specific safe browsing client name.
std::string GetSafeBrowsingName();

// Returns true if usage stats collecting is enabled for this user for the
// current executable.
bool GetCollectStatsConsent();

// Returns true if the current executable is currently in the chosen sample that
// will report stats and crashes.
bool GetCollectStatsInSample();

// Sets the registry value used for checking if Chrome is in the chosen sample
// that will report stats and crashes. Returns true if writing was successful.
bool SetCollectStatsInSample(bool in_sample);

// Appends "[kCompanyPathName\]kProductPathName[install_suffix]" to |path|,
// returning a reference to |path|.
std::wstring& AppendChromeInstallSubDirectory(const InstallConstants& mode,
                                              bool include_suffix,
                                              std::wstring* path);

// Returns true if if usage stats reporting is controlled by a mandatory
// policy. |crash_reporting_enabled| determines whether it's enabled (true) or
// disabled (false).
bool ReportingIsEnforcedByPolicy(bool* crash_reporting_enabled);

// Initializes |g_process_type| which stores whether or not the current
// process is the main browser process.
void InitializeProcessType();

// Returns true if the process type is initialized. False otherwise.
bool IsProcessTypeInitialized();

// Returns true if invoked in the main browser process; false, otherwise.
bool IsBrowserProcess();

// Returns true if invoked in a Crashpad handler process. False otherwise.
bool IsCrashpadHandlerProcess();

// Returns true if the |process_type| has the rights to access the profile.
// False otherwise.
bool ProcessNeedsProfileDir(const std::string& process_type);

// Populates |crash_dir| with the crash dump location, respecting modifications
// to user-data-dir.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
std::wstring GetCrashDumpLocation();

// Returns the contents of the specified |variable_name| from the environment
// block of the calling process. Returns an empty string if the variable does
// not exist.
std::string GetEnvironmentString(const std::string& variable_name);
std::wstring GetEnvironmentString(const wchar_t* variable_name);

// Sets the environment variable identified by |variable_name| to the value
// identified by |new_value|.
bool SetEnvironmentString(const std::string& variable_name,
                          const std::string& new_value);
bool SetEnvironmentString(const std::wstring& variable_name,
                          const std::wstring& new_value);

// Returns true if the environment variable identified by |variable_name|
// exists.
bool HasEnvironmentVariable(const std::string& variable_name);
bool HasEnvironmentVariable(const std::wstring& variable_name);

// Gets the exe version details like the |product_name|, |version|,
// |special_build|, and |channel_name| from the browser executable at
// |exe_path|. |channel_name| will be "extended" for clients that follow the
// extended stable update channel.
void GetExecutableVersionDetails(const std::wstring& exe_path,
                                 std::wstring* product_name,
                                 std::wstring* version,
                                 std::wstring* special_build,
                                 std::wstring* channel_name);

// Gets the channel for the current Chrome process.
version_info::Channel GetChromeChannel();

// Gets the channel for the current Chrome process. Unless
// `with_extended_stable` is true, extended stable will be reported as regular
// stable (i.e., an empty string).
std::wstring GetChromeChannelName(bool with_extended_stable);

// Returns true if the current Chrome process is on the extended stable channel.
bool IsExtendedStableChannel();

// UTF8 to Wide and vice versa conversion helpers.
std::wstring UTF8ToWide(const std::string& source);

std::string WideToUTF8(const std::wstring& source);

// Tokenizes a string |str| based on single character delimiter.
// The tokens are returned in a vector. The |trim_spaces| parameter indicates
// whether the function should optionally trim spaces throughout the string.
std::vector<std::string> TokenizeString(const std::string& str,
                                        char delimiter,
                                        bool trim_spaces);
std::vector<std::wstring> TokenizeString(const std::wstring& str,
                                         wchar_t delimiter,
                                         bool trim_spaces);

// Tokenizes |command_line| in the same way as CommandLineToArgvW() in
// shell32.dll, handling quoting, spacing etc. Normally only used from
// GetCommandLineSwitch(), but exposed for testing.
std::vector<std::wstring> TokenizeCommandLineToArray(
    const std::wstring& command_line);

// Returns the value of a switch of the form "--<switch name>=<switch value>" in
// |command_line|. If the switch has no value, returns an empty string. If the
// switch is not present returns std::nullopt. An empty switch in |command_line|
// ("--") denotes the end of switches and the beginning of args. Anything
// following the "--" switch is ignored.
std::optional<std::wstring> GetCommandLineSwitch(
    const std::wstring& command_line,
    std::wstring_view switch_name);

// Returns the value of the specified switch or an empty string if there is no
// such switch in |command_line| or the switch has no value.
std::wstring GetCommandLineSwitchValue(const std::wstring& command_line,
                                       std::wstring_view switch_name);

// Ensures that the given |full_path| exists, and that the tail component is a
// directory. If the directory does not already exist, it will be created.
// Returns false if the final component exists but is not a directory, or on
// failure to create a directory.
bool RecursiveDirectoryCreate(const std::wstring& full_path);

// Creates a new directory with the unique name in the format of
// <prefix>[Chrome|Chromium]<random number> in the default %TEMP% folder.
// If the directory cannot be created, returns an empty string.
std::wstring CreateUniqueTempDirectory(std::wstring_view prefix);

struct DetermineChannelResult {
  std::wstring channel_name;
  ChannelOrigin origin;

  // True if this client follows the extended stable update channel. May only be
  // true if `channel_name` is "" and `origin` is kPolicy.
  bool is_extended_stable;
};

// Returns the unadorned channel name, its origin, and an indication of whether
// or not a stable ("") channel is truly the extended stable channel based on
// the channel strategy for the install mode. |channel_override|, if not empty
// is the channel to return if |mode| supports non-fixed channels. |update_ap|,
// if not null, is set to the raw "ap" value read from Chrome's ClientState key
// in the registry. |update_cohort_name|, if not null, is set to the raw
// "cohort\name" value read from Chrome's ClientState key in the registry.
DetermineChannelResult DetermineChannel(const InstallConstants& mode,
                                        bool system_level,
                                        const wchar_t* channel_override,
                                        std::wstring* update_ap,
                                        std::wstring* update_cohort_name);

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
