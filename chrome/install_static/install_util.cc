// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>

#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/policy_path_parser.h"
#include "chrome/install_static/user_data_dir.h"
#include "components/nacl/common/buildflags.h"
#include "components/version_info/channel.h"

namespace install_static {

enum class ProcessType {
  UNINITIALIZED,
  OTHER_PROCESS,
  BROWSER_PROCESS,
  CLOUD_PRINT_SERVICE_PROCESS,
#if BUILDFLAG(ENABLE_NACL)
  NACL_BROKER_PROCESS,
  NACL_LOADER_PROCESS,
#endif
};

// Caches the |ProcessType| of the current process.
ProcessType g_process_type = ProcessType::UNINITIALIZED;

const wchar_t kRegValueChromeStatsSample[] = L"UsageStatsInSample";

// TODO(ananta)
// http://crbug.com/604923
// The constants defined in this file are also defined in chrome/installer and
// other places. we need to unify them.
const wchar_t kHeadless[] = L"CHROME_HEADLESS";
const wchar_t kShowRestart[] = L"CHROME_CRASHED";
const wchar_t kRestartInfo[] = L"CHROME_RESTART";
const wchar_t kRtlLocale[] = L"RIGHT_TO_LEFT";

const wchar_t kCrashpadHandler[] = L"crashpad-handler";
const wchar_t kFallbackHandler[] = L"fallback-handler";

const wchar_t kProcessType[] = L"type";
const wchar_t kUserDataDirSwitch[] = L"user-data-dir";
const wchar_t kUtilityProcess[] = L"utility";

namespace {

// TODO(ananta)
// http://crbug.com/604923
// The constants defined in this file are also defined in chrome/installer and
// other places. we need to unify them.
// Chrome channel display names.
constexpr wchar_t kChromeChannelDev[] = L"dev";
constexpr wchar_t kChromeChannelBeta[] = L"beta";
constexpr wchar_t kChromeChannelStableExplicit[] = L"stable";

// TODO(ananta)
// http://crbug.com/604923
// These constants are defined in the chrome/installer directory as well. We
// need to unify them.
constexpr wchar_t kRegValueAp[] = L"ap";
constexpr wchar_t kRegValueName[] = L"name";
constexpr wchar_t kRegValueUsageStats[] = L"usagestats";
constexpr wchar_t kMetricsReportingEnabled[] = L"MetricsReportingEnabled";

constexpr wchar_t kCloudPrintServiceProcess[] = L"service";
#if BUILDFLAG(ENABLE_NACL)
constexpr wchar_t kNaClBrokerProcess[] = L"nacl-broker";
constexpr wchar_t kNaClLoaderProcess[] = L"nacl-loader";
#endif

void Trace(const wchar_t* format_string, ...) {
  static const int kMaxLogBufferSize = 1024;
  static wchar_t buffer[kMaxLogBufferSize] = {};

  va_list args = {};

  va_start(args, format_string);
  vswprintf(buffer, kMaxLogBufferSize, format_string, args);
  OutputDebugStringW(buffer);
  va_end(args);
}

bool GetLanguageAndCodePageFromVersionResource(const char* version_resource,
                                               WORD* language,
                                               WORD* code_page) {
  if (!version_resource)
    return false;

  struct LanguageAndCodePage {
    WORD language;
    WORD code_page;
  };

  LanguageAndCodePage* translation_info = nullptr;
  uint32_t data_size_in_bytes = 0;
  BOOL query_result = VerQueryValueW(
      version_resource, L"\\VarFileInfo\\Translation",
      reinterpret_cast<void**>(&translation_info), &data_size_in_bytes);
  if (!query_result)
    return false;

  *language = translation_info->language;
  *code_page = translation_info->code_page;
  return true;
}

bool GetValueFromVersionResource(const char* version_resource,
                                 const std::wstring& name,
                                 std::wstring* value_str) {
  assert(value_str);
  value_str->clear();

  // TODO(ananta)
  // It may be better in the long run to enumerate the languages and code pages
  // in the version resource and return the value from the first match.
  WORD language = 0;
  WORD code_page = 0;
  if (!GetLanguageAndCodePageFromVersionResource(version_resource, &language,
                                                 &code_page)) {
    return false;
  }

  const size_t array_size = 8;
  WORD lang_codepage[array_size] = {};
  size_t i = 0;
  // Use the language and codepage
  lang_codepage[i++] = language;
  lang_codepage[i++] = code_page;
  // Use the default language and codepage from the resource.
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = code_page;
  // Use the language from the resource and Latin codepage (most common).
  lang_codepage[i++] = language;
  lang_codepage[i++] = 1252;
  // Use the default language and Latin codepage (most common).
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = 1252;

  static_assert((array_size % 2) == 0,
                "Language code page size should be a multiple of 2");
  assert(array_size == i);

  for (i = 0; i < array_size;) {
    wchar_t sub_block[MAX_PATH];
    WORD language = lang_codepage[i++];
    WORD code_page = lang_codepage[i++];
    _snwprintf_s(sub_block, MAX_PATH, MAX_PATH,
                 L"\\StringFileInfo\\%04hx%04hx\\%ls", language, code_page,
                 name.c_str());
    void* value = nullptr;
    uint32_t size = 0;
    BOOL r = ::VerQueryValueW(version_resource, sub_block, &value, &size);
    if (r && value) {
      value_str->assign(static_cast<wchar_t*>(value));
      return true;
    }
  }
  return false;
}

bool DirectoryExists(const std::wstring& path) {
  DWORD file_attributes = ::GetFileAttributes(path.c_str());
  if (file_attributes == INVALID_FILE_ATTRIBUTES)
    return false;
  return (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Returns true if the |source| string matches the |pattern|. The pattern
// may contain wildcards like '?' which matches one character or a '*'
// which matches 0 or more characters.
// Please note that pattern matches the whole string. If you want to find
// something in the middle of the string then you need to specify the pattern
// as '*xyz*'.
// |source_index| is the index of the current character being matched in
// |source|.
// |pattern_index| is the index of the current pattern character in |pattern|
// which is matched with source.
bool MatchPatternImpl(const std::wstring& source,
                      const std::wstring& pattern,
                      size_t source_index,
                      size_t pattern_index) {
  if (source.empty() && pattern.empty())
    return true;

  if (source_index > source.length() || pattern_index > pattern.length())
    return false;

  // If we reached the end of both strings, then we are done.
  if ((source_index == source.length()) &&
      (pattern_index == pattern.length())) {
    return true;
  }

  // If the current character in the pattern is a '*' then make sure that
  // characters after the pattern are present in the source string. This
  // assumes that you won't have two consecutive '*' characters in the pattern.
  if ((pattern[pattern_index] == L'*') &&
      (pattern_index + 1 < pattern.length()) &&
      (source_index >= source.length())) {
    return false;
  }

  // If the pattern contains wildcard characters '?' or '.' or there is a match
  // then move ahead in both strings.
  if ((pattern[pattern_index] == L'?') ||
      (pattern[pattern_index] == source[source_index])) {
    return MatchPatternImpl(source, pattern, source_index + 1,
                            pattern_index + 1);
  }

  // If we have a '*' then there are two possibilities
  // 1. We consider current character of source.
  // 2. We ignore current character of source.
  if (pattern[pattern_index] == L'*') {
    return MatchPatternImpl(source, pattern, source_index + 1, pattern_index) ||
           MatchPatternImpl(source, pattern, source_index, pattern_index + 1);
  }
  return false;
}

// Defines the type of whitespace characters typically found in strings.
constexpr char kWhiteSpaces[] = " \t\n\r\f\v";
constexpr wchar_t kWhiteSpaces16[] = L" \t\n\r\f\v";

// Define specializations for white spaces based on the type of the string.
template <class StringType>
StringType GetWhiteSpacesForType();
template <>
std::wstring GetWhiteSpacesForType() {
  return kWhiteSpaces16;
}
template <>
std::string GetWhiteSpacesForType() {
  return kWhiteSpaces;
}

// Trim whitespaces from left & right
template <class StringType>
void TrimT(StringType* str) {
  str->erase(str->find_last_not_of(GetWhiteSpacesForType<StringType>()) + 1);
  str->erase(0, str->find_first_not_of(GetWhiteSpacesForType<StringType>()));
}

// Tokenizes a string based on a single character delimiter.
template <class StringType>
std::vector<StringType> TokenizeStringT(
    const StringType& str,
    typename StringType::value_type delimiter,
    bool trim_spaces) {
  std::vector<StringType> tokens;
  std::basic_istringstream<typename StringType::value_type> buffer(str);
  for (StringType token; std::getline(buffer, token, delimiter);) {
    if (trim_spaces)
      TrimT<StringType>(&token);
    tokens.push_back(token);
  }
  return tokens;
}

// Returns Chrome's update channel name based on the contents of the given "ap"
// value from Chrome's ClientState key.
std::wstring ChannelFromAdditionalParameters(const InstallConstants& mode,
                                             const std::wstring& ap_value) {
  assert(kUseGoogleUpdateIntegration);

  static constexpr wchar_t kChromeChannelBetaPattern[] = L"1?1-*";
  static constexpr wchar_t kChromeChannelBetaX64Pattern[] = L"*x64-beta*";
  static constexpr wchar_t kChromeChannelDevPattern[] = L"2?0-d*";
  static constexpr wchar_t kChromeChannelDevX64Pattern[] = L"*x64-dev*";

  std::wstring value;
  value.reserve(ap_value.size());
  std::transform(ap_value.begin(), ap_value.end(), std::back_inserter(value),
                 ::tolower);

  // Empty channel names or those containing "stable" should be reported as
  // an empty string.
  if (value.empty() ||
      (value.find(kChromeChannelStableExplicit) != std::wstring::npos)) {
    return std::wstring();
  }
  if (MatchPattern(value, kChromeChannelDevPattern) ||
      MatchPattern(value, kChromeChannelDevX64Pattern)) {
    return kChromeChannelDev;
  }
  if (MatchPattern(value, kChromeChannelBetaPattern) ||
      MatchPattern(value, kChromeChannelBetaX64Pattern)) {
    return kChromeChannelBeta;
  }
  // Else report values with garbage as stable since they will match the stable
  // rules in the update configs.
  return std::wstring();
}

// Converts a process type specified as a string to the ProcessType enum.
ProcessType GetProcessType(const std::wstring& process_type) {
  if (process_type.empty())
    return ProcessType::BROWSER_PROCESS;
  if (process_type == kCloudPrintServiceProcess)
    return ProcessType::CLOUD_PRINT_SERVICE_PROCESS;
#if BUILDFLAG(ENABLE_NACL)
  if (process_type == kNaClBrokerProcess)
    return ProcessType::NACL_BROKER_PROCESS;
  if (process_type == kNaClLoaderProcess)
    return ProcessType::NACL_LOADER_PROCESS;
#endif
  return ProcessType::OTHER_PROCESS;
}

// Returns whether |process_type| needs the profile directory.
bool ProcessNeedsProfileDir(ProcessType process_type) {
  // On Windows we don't want subprocesses other than the browser process and
  // service processes to be able to use the profile directory because if it
  // lies on a network share the sandbox will prevent us from accessing it.
  switch (process_type) {
    case ProcessType::BROWSER_PROCESS:
    case ProcessType::CLOUD_PRINT_SERVICE_PROCESS:
#if BUILDFLAG(ENABLE_NACL)
    case ProcessType::NACL_BROKER_PROCESS:
    case ProcessType::NACL_LOADER_PROCESS:
#endif
      return true;
    case ProcessType::OTHER_PROCESS:
      return false;
    case ProcessType::UNINITIALIZED:
      assert(false);
      return false;
  }
  assert(false);
  return false;
}

}  // namespace

bool IsSystemInstall() {
  return InstallDetails::Get().system_level();
}

std::wstring GetChromeInstallSubDirectory() {
  std::wstring result;
  AppendChromeInstallSubDirectory(InstallDetails::Get().mode(),
                                  true /* include_suffix */, &result);
  return result;
}

std::wstring GetRegistryPath() {
  std::wstring result(L"Software\\");
  AppendChromeInstallSubDirectory(InstallDetails::Get().mode(),
                                  true /* include_suffix */, &result);
  return result;
}

std::wstring GetClientsKeyPath() {
  return GetClientsKeyPath(GetAppGuid());
}

std::wstring GetClientStateKeyPath() {
  return GetClientStateKeyPath(GetAppGuid());
}

std::wstring GetClientStateMediumKeyPath() {
  return GetClientStateMediumKeyPath(GetAppGuid());
}

std::wstring GetClientStateKeyPathForBinaries() {
  return GetBinariesClientStateKeyPath();
}

std::wstring GetClientStateMediumKeyPathForBinaries() {
  return GetBinariesClientStateMediumKeyPath();
}

std::wstring GetUninstallRegistryPath() {
  std::wstring result(
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
  if (*kCompanyPathName)
    result.append(kCompanyPathName).append(1, L' ');
  result.append(kProductPathName, kProductPathNameLength);
  return result.append(InstallDetails::Get().mode().install_suffix);
}

const wchar_t* GetAppGuid() {
  return InstallDetails::Get().app_guid();
}

const CLSID& GetToastActivatorClsid() {
  return InstallDetails::Get().toast_activator_clsid();
}

const CLSID& GetElevatorClsid() {
  return InstallDetails::Get().elevator_clsid();
}

const CLSID& GetElevatorIid() {
  return InstallDetails::Get().elevator_iid();
}

std::wstring GetElevationServiceName() {
  std::wstring name = GetElevationServiceDisplayName();
  name.erase(std::remove_if(name.begin(), name.end(), isspace), name.end());
  return name;
}

std::wstring GetElevationServiceDisplayName() {
  static constexpr wchar_t kElevationServiceDisplayName[] =
      L" Elevation Service";
  return GetBaseAppName() + kElevationServiceDisplayName;
}

std::wstring GetBaseAppName() {
  return InstallDetails::Get().mode().base_app_name;
}

const wchar_t* GetBaseAppId() {
  return InstallDetails::Get().base_app_id();
}

const wchar_t* GetProgIdPrefix() {
  return InstallDetails::Get().mode().prog_id_prefix;
}

const wchar_t* GetProgIdDescription() {
  return InstallDetails::Get().mode().prog_id_description;
}

std::wstring GetActiveSetupPath() {
  return std::wstring(
             L"Software\\Microsoft\\Active Setup\\Installed Components\\")
      .append(InstallDetails::Get().mode().active_setup_guid);
}

std::wstring GetLegacyCommandExecuteImplClsid() {
  return InstallDetails::Get().mode().legacy_command_execute_clsid;
}

bool SupportsSetAsDefaultBrowser() {
  return InstallDetails::Get().mode().supports_set_as_default_browser;
}

bool SupportsRetentionExperiments() {
  return InstallDetails::Get().mode().supports_retention_experiments;
}

int GetIconResourceIndex() {
  return InstallDetails::Get().mode().app_icon_resource_index;
}

const wchar_t* GetSandboxSidPrefix() {
  return InstallDetails::Get().mode().sandbox_sid_prefix;
}

std::string GetSafeBrowsingName() {
  return kSafeBrowsingName;
}

bool GetCollectStatsConsent() {
  bool enabled = true;

  if (ReportingIsEnforcedByPolicy(&enabled))
    return enabled;

  const bool system_install = IsSystemInstall();

  DWORD out_value = 0;

  // If system_install, first try ClientStateMedium in HKLM.
  if (system_install &&
      nt::QueryRegValueDWORD(
          nt::HKLM, nt::WOW6432,
          InstallDetails::Get().GetClientStateMediumKeyPath().c_str(),
          kRegValueUsageStats, &out_value)) {
    return (out_value == 1);
  }

  // Second, try ClientState.
  return (nt::QueryRegValueDWORD(
              system_install ? nt::HKLM : nt::HKCU, nt::WOW6432,
              InstallDetails::Get().GetClientStateKeyPath().c_str(),
              kRegValueUsageStats, &out_value) &&
          out_value == 1);
}

bool GetCollectStatsInSample() {
  std::wstring registry_path = GetRegistryPath();

  DWORD out_value = 0;
  if (!nt::QueryRegValueDWORD(nt::HKCU, nt::WOW6432, registry_path.c_str(),
                              kRegValueChromeStatsSample, &out_value)) {
    // If reading the value failed, treat it as though sampling isn't in effect,
    // implicitly meaning this install is in the sample.
    return true;
  }
  return out_value == 1;
}

bool SetCollectStatsInSample(bool in_sample) {
  std::wstring registry_path = GetRegistryPath();

  HANDLE key_handle = INVALID_HANDLE_VALUE;
  if (!nt::CreateRegKey(nt::HKCU, registry_path.c_str(),
                        KEY_SET_VALUE | KEY_WOW64_32KEY, &key_handle)) {
    return false;
  }

  bool success = nt::SetRegValueDWORD(key_handle, kRegValueChromeStatsSample,
                                      in_sample ? 1 : 0);
  nt::CloseRegKey(key_handle);
  return success;
}

// Appends "[kCompanyPathName\]kProductPathName[install_suffix]" to |path|,
// returning a reference to |path|.
std::wstring& AppendChromeInstallSubDirectory(const InstallConstants& mode,
                                              bool include_suffix,
                                              std::wstring* path) {
  if (*kCompanyPathName) {
    path->append(kCompanyPathName);
    path->push_back(L'\\');
  }
  path->append(kProductPathName, kProductPathNameLength);
  if (!include_suffix)
    return *path;
  return path->append(mode.install_suffix);
}

bool ReportingIsEnforcedByPolicy(bool* crash_reporting_enabled) {
  std::wstring policies_path = L"SOFTWARE\\Policies\\";
  AppendChromeInstallSubDirectory(InstallDetails::Get().mode(),
                                  false /* !include_suffix */, &policies_path);
  DWORD value = 0;

  // First, try HKLM.
  if (nt::QueryRegValueDWORD(nt::HKLM, nt::NONE, policies_path.c_str(),
                             kMetricsReportingEnabled, &value)) {
    *crash_reporting_enabled = (value != 0);
    return true;
  }

  // Second, try HKCU.
  if (nt::QueryRegValueDWORD(nt::HKCU, nt::NONE, policies_path.c_str(),
                             kMetricsReportingEnabled, &value)) {
    *crash_reporting_enabled = (value != 0);
    return true;
  }

  return false;
}

void InitializeProcessType() {
  assert(g_process_type == ProcessType::UNINITIALIZED);
  std::wstring process_type =
      GetSwitchValueFromCommandLine(::GetCommandLine(), kProcessType);
  g_process_type = GetProcessType(process_type);
}

bool IsProcessTypeInitialized() {
  return g_process_type != ProcessType::UNINITIALIZED;
}

bool IsNonBrowserProcess() {
  assert(g_process_type != ProcessType::UNINITIALIZED);
  return g_process_type != ProcessType::BROWSER_PROCESS;
}

bool ProcessNeedsProfileDir(const std::string& process_type) {
  return ProcessNeedsProfileDir(GetProcessType(UTF8ToUTF16(process_type)));
}

std::wstring GetCrashDumpLocation() {
  // In order to be able to start crash handling very early and in chrome_elf,
  // we cannot rely on chrome's PathService entries (for DIR_CRASH_DUMPS) being
  // available on Windows. See https://crbug.com/564398.
  std::wstring user_data_dir;
  bool ret = GetUserDataDirectory(&user_data_dir, nullptr);
  assert(ret);
  IgnoreUnused(ret);
  return user_data_dir.append(L"\\Crashpad");
}

std::string GetEnvironmentString(const std::string& variable_name) {
  return UTF16ToUTF8(
      GetEnvironmentString16(UTF8ToUTF16(variable_name).c_str()));
}

std::wstring GetEnvironmentString16(const wchar_t* variable_name) {
  DWORD value_length = ::GetEnvironmentVariableW(variable_name, nullptr, 0);
  if (!value_length)
    return std::wstring();
  std::wstring value(value_length, L'\0');
  value_length =
      ::GetEnvironmentVariableW(variable_name, &value[0], value_length);
  if (!value_length || value_length >= value.size())
    return std::wstring();
  value.resize(value_length);
  return value;
}

bool SetEnvironmentString(const std::string& variable_name,
                          const std::string& new_value) {
  return SetEnvironmentString16(UTF8ToUTF16(variable_name),
                                UTF8ToUTF16(new_value));
}

bool SetEnvironmentString16(const std::wstring& variable_name,
                            const std::wstring& new_value) {
  return !!SetEnvironmentVariable(variable_name.c_str(), new_value.c_str());
}

bool HasEnvironmentVariable(const std::string& variable_name) {
  return HasEnvironmentVariable16(UTF8ToUTF16(variable_name));
}

bool HasEnvironmentVariable16(const std::wstring& variable_name) {
  return !!::GetEnvironmentVariable(variable_name.c_str(), nullptr, 0);
}

void GetExecutableVersionDetails(const std::wstring& exe_path,
                                 std::wstring* product_name,
                                 std::wstring* version,
                                 std::wstring* special_build,
                                 std::wstring* channel_name) {
  assert(product_name);
  assert(version);
  assert(special_build);
  assert(channel_name);

  // Default values in case we don't find a version resource.
  *product_name = L"Chrome";
  *version = L"0.0.0.0-devel";
  special_build->clear();

  DWORD dummy = 0;
  DWORD length = ::GetFileVersionInfoSize(exe_path.c_str(), &dummy);
  if (length) {
    std::unique_ptr<char[]> data(new char[length]);
    if (::GetFileVersionInfo(exe_path.c_str(), dummy, length, data.get())) {
      GetValueFromVersionResource(data.get(), L"ProductVersion", version);

      std::wstring official_build;
      GetValueFromVersionResource(data.get(), L"Official Build",
                                  &official_build);
      if (official_build != L"1")
        version->append(L"-devel");
      GetValueFromVersionResource(data.get(), L"ProductShortName",
                                  product_name);
      GetValueFromVersionResource(data.get(), L"SpecialBuild", special_build);
    }
  }
  *channel_name = GetChromeChannelName();
}

version_info::Channel GetChromeChannel() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::wstring channel_name(GetChromeChannelName());
  if (channel_name.empty()) {
    return version_info::Channel::STABLE;
  }
  if (channel_name == L"beta") {
    return version_info::Channel::BETA;
  }
  if (channel_name == L"dev") {
    return version_info::Channel::DEV;
  }
  if (channel_name == L"canary") {
    return version_info::Channel::CANARY;
  }
#endif

  return version_info::Channel::UNKNOWN;
}

std::wstring GetChromeChannelName() {
  return InstallDetails::Get().channel();
}

bool MatchPattern(const std::wstring& source, const std::wstring& pattern) {
  assert(pattern.find(L"**") == std::wstring::npos);
  return MatchPatternImpl(source, pattern, 0, 0);
}

std::string UTF16ToUTF8(const std::wstring& source) {
  if (source.empty() ||
      static_cast<int>(source.size()) > std::numeric_limits<int>::max()) {
    return std::string();
  }
  int size = ::WideCharToMultiByte(CP_UTF8, 0, &source[0],
                                   static_cast<int>(source.size()), nullptr, 0,
                                   nullptr, nullptr);
  std::string result(size, '\0');
  if (::WideCharToMultiByte(CP_UTF8, 0, &source[0],
                            static_cast<int>(source.size()), &result[0], size,
                            nullptr, nullptr) != size) {
    assert(false);
    return std::string();
  }
  return result;
}

std::wstring UTF8ToUTF16(const std::string& source) {
  if (source.empty() ||
      static_cast<int>(source.size()) > std::numeric_limits<int>::max()) {
    return std::wstring();
  }
  int size = ::MultiByteToWideChar(CP_UTF8, 0, &source[0],
                                   static_cast<int>(source.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  if (::MultiByteToWideChar(CP_UTF8, 0, &source[0],
                            static_cast<int>(source.size()), &result[0],
                            size) != size) {
    assert(false);
    return std::wstring();
  }
  return result;
}

std::vector<std::string> TokenizeString(const std::string& str,
                                        char delimiter,
                                        bool trim_spaces) {
  return TokenizeStringT<std::string>(str, delimiter, trim_spaces);
}

std::vector<std::wstring> TokenizeString16(const std::wstring& str,
                                           wchar_t delimiter,
                                           bool trim_spaces) {
  return TokenizeStringT<std::wstring>(str, delimiter, trim_spaces);
}

std::vector<std::wstring> TokenizeCommandLineToArray(
    const std::wstring& command_line) {
  // This is baroquely complex to do properly, see e.g.
  // https://blogs.msdn.microsoft.com/oldnewthing/20100917-00/?p=12833
  // http://www.windowsinspired.com/how-a-windows-programs-splits-its-command-line-into-individual-arguments/
  // and many others. We cannot use CommandLineToArgvW() in chrome_elf, because
  // it's in shell32.dll. Previously, __wgetmainargs() in the CRT was available,
  // and it's still documented for VS 2015 at
  // https://msdn.microsoft.com/en-us/library/ff770599.aspx but unfortunately,
  // isn't actually available.
  //
  // This parsing matches CommandLineToArgvW()s for arguments, rather than the
  // CRTs. These are different only in the most obscure of cases and will not
  // matter in any practical situation. See the windowsinspired.com post above
  // for details.
  //
  // Indicates whether or not space and tab are interpreted as token separators.
  enum class SpecialChars {
    // Space or tab, if encountered, delimit tokens.
    kInterpret,

    // Space or tab, if encountered, are part of the current token.
    kIgnore,
  } state;

  static constexpr wchar_t kSpaceTab[] = L" \t";

  std::vector<std::wstring> result;
  const wchar_t* p = command_line.c_str();

  // The first argument (the program) is delimited by whitespace or quotes based
  // on its first character.
  size_t argv0_length = 0;
  if (p[0] == L'"') {
    const wchar_t* closing = wcschr(++p, L'"');
    if (!closing)
      argv0_length = command_line.size() - 1;  // Skip the opening quote.
    else
      argv0_length = closing - (command_line.c_str() + 1);
  } else {
    argv0_length = wcscspn(p, kSpaceTab);
  }
  result.emplace_back(p, argv0_length);
  if (p[argv0_length] == 0)
    return result;
  p += argv0_length + 1;

  std::wstring token;
  // This loops the entire string, with a subloop for each argument.
  for (;;) {
    // Advance past leading whitespace (only space and tab are handled).
    p += wcsspn(p, kSpaceTab);

    // End of arguments.
    if (p[0] == 0)
      break;

    state = SpecialChars::kInterpret;

    // Scan an argument.
    for (;;) {
      // Count and advance past collections of backslashes, which have special
      // meaning when followed by a double quote.
      int num_backslashes = wcsspn(p, L"\\");
      p += num_backslashes;

      if (p[0] == L'"') {
        // Emit a backslash for each pair of backslashes found. A non-paired
        // "extra" backslash is handled below.
        token.append(num_backslashes / 2, L'\\');

        if (num_backslashes % 2 == 1) {
          // An odd number of backslashes followed by a quote is treated as
          // pairs of protected backslashes, followed by the protected quote.
          token += L'"';
        } else if (p[1] == L'"' && state == SpecialChars::kIgnore) {
          // Special case for consecutive double quotes within a quoted string:
          // emit one for the pair, and switch back to interpreting special
          // characters.
          ++p;
          token += L'"';
          state = SpecialChars::kInterpret;
        } else {
          state = state == SpecialChars::kInterpret ? SpecialChars::kIgnore
                                                    : SpecialChars::kInterpret;
        }
      } else {
        // Emit backslashes that do not precede a quote verbatim.
        token.append(num_backslashes, L'\\');
        if (p[0] == 0 ||
            (state == SpecialChars::kInterpret && wcschr(kSpaceTab, p[0]))) {
          result.push_back(token);
          token.clear();
          break;
        }

        token += *p;
      }

      ++p;
    }
  }

  return result;
}

std::wstring GetSwitchValueFromCommandLine(const std::wstring& command_line,
                                           const std::wstring& switch_name) {
  static constexpr wchar_t kSwitchTerminator[] = L"--";
  assert(!command_line.empty());
  assert(!switch_name.empty());

  std::vector<std::wstring> as_array = TokenizeCommandLineToArray(command_line);
  std::wstring switch_with_equal = L"--" + switch_name + L"=";
  auto end = std::find(as_array.cbegin(), as_array.cend(), kSwitchTerminator);
  for (auto scan = as_array.cbegin(); scan != end; ++scan) {
    const std::wstring& arg = *scan;
    if (arg.compare(0, switch_with_equal.size(), switch_with_equal) == 0)
      return arg.substr(switch_with_equal.size());
  }

  return std::wstring();
}

bool RecursiveDirectoryCreate(const std::wstring& full_path) {
  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* full_path_str = full_path.c_str();
  DWORD file_attributes = ::GetFileAttributes(full_path_str);
  if (file_attributes != INVALID_FILE_ATTRIBUTES) {
    if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      Trace(L"%hs( %ls directory exists )\n", __func__, full_path_str);
      return true;
    }
    Trace(L"%hs( %ls directory conflicts with an existing file. )\n", __func__,
          full_path_str);
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  std::wstring parent_path;
  std::size_t pos = full_path.find_last_of(L"/\\");
  if (pos != std::wstring::npos) {
    parent_path = full_path.substr(0, pos);
    if (!RecursiveDirectoryCreate(parent_path)) {
      Trace(L"Failed to create one of the parent directories");
      return false;
    }
  }
  if (!::CreateDirectory(full_path_str, nullptr)) {
    DWORD error_code = ::GetLastError();
    if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path_str)) {
      // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we
      // were racing with someone creating the same directory, or a file
      // with the same path.  If the directory exists, we lost the
      // race to create the same directory.
      return true;
    } else {
      Trace(L"Failed to create directory %ls, last error is %d\n",
            full_path_str, error_code);
      return false;
    }
  }
  return true;
}

// This function takes these inputs rather than accessing the module's
// InstallDetails instance since it is used to bootstrap InstallDetails.
std::wstring DetermineChannel(const InstallConstants& mode,
                              bool system_level,
                              bool from_binaries,
                              std::wstring* update_ap,
                              std::wstring* update_cohort_name) {
  if (!kUseGoogleUpdateIntegration)
    return std::wstring();

  // Read the "ap" value and cache it if requested.
  std::wstring client_state(from_binaries
                                ? GetBinariesClientStateKeyPath()
                                : GetClientStateKeyPath(mode.app_guid));
  std::wstring ap_value;
  // An empty |ap_value| is used in case of error.
  nt::QueryRegValueSZ(system_level ? nt::HKLM : nt::HKCU, nt::WOW6432,
                      client_state.c_str(), kRegValueAp, &ap_value);
  if (update_ap)
    *update_ap = ap_value;

  // Cache the cohort name if requested.
  if (update_cohort_name) {
    nt::QueryRegValueSZ(system_level ? nt::HKLM : nt::HKCU, nt::WOW6432,
                        client_state.append(L"\\cohort").c_str(), kRegValueName,
                        update_cohort_name);
  }

  switch (mode.channel_strategy) {
    case ChannelStrategy::UNSUPPORTED:
      assert(false);
      break;
    case ChannelStrategy::ADDITIONAL_PARAMETERS:
      return ChannelFromAdditionalParameters(mode, ap_value);
    case ChannelStrategy::FIXED:
      return mode.default_channel_name;
  }

  return std::wstring();
}

}  // namespace install_static
