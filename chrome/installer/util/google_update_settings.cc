// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/additional_parameters.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"

using base::win::RegKey;
using installer::InstallationState;

const wchar_t GoogleUpdateSettings::kPoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const wchar_t GoogleUpdateSettings::kUpdatePolicyValue[] = L"UpdateDefault";
const wchar_t GoogleUpdateSettings::kDownloadPreferencePolicyValue[] =
    L"DownloadPreference";
const wchar_t GoogleUpdateSettings::kUpdateOverrideValuePrefix[] = L"Update";
const wchar_t GoogleUpdateSettings::kCheckPeriodOverrideMinutes[] =
    L"AutoUpdateCheckPeriodMinutes";

// Don't allow update periods longer than six weeks.
const int GoogleUpdateSettings::kCheckPeriodOverrideMinutesMax =
    60 * 24 * 7 * 6;

const GoogleUpdateSettings::UpdatePolicy
    GoogleUpdateSettings::kDefaultUpdatePolicy =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        GoogleUpdateSettings::AUTOMATIC_UPDATES;
#else
        GoogleUpdateSettings::UPDATES_DISABLED;
#endif

namespace {

base::LazyThreadPoolSequencedTaskRunner g_collect_stats_consent_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

// Reads the value |name| from the app's ClientState registry key in |root| into
// |value|.
bool ReadGoogleUpdateStrKeyFromRoot(HKEY root,
                                    const wchar_t* const name,
                                    std::wstring* value) {
  RegKey key;
  return key.Open(root, install_static::GetClientStateKeyPath().c_str(),
                  KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
         key.ReadValue(name, value) == ERROR_SUCCESS;
}

// Returns the value |name| from the app's ClientState cohort registry key in
// |root|.
std::optional<std::wstring> ReadGoogleUpdateCohortStrKeyFromRoot(
    HKEY root,
    const wchar_t* const name) {
  std::wstring value;
  RegKey key;
  if (key.Open(
          root,
          install_static::GetClientStateKeyPath().append(L"\\cohort").c_str(),
          KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(name, &value) == ERROR_SUCCESS) {
    return value;
  }
  return std::nullopt;
}

// Reads the value |name| from the app's ClientState registry key in
// HKEY_CURRENT_USER into |value|. This function is only provided for legacy
// use. New code needing to load/store per-user data should use
// install_details::GetRegistryPath().
bool ReadUserGoogleUpdateStrKey(const wchar_t* const name,
                                std::wstring* value) {
  return ReadGoogleUpdateStrKeyFromRoot(HKEY_CURRENT_USER, name, value);
}

// Reads the value |name| from the app's ClientState registry key into |value|.
// This is primarily to be used for reading values written by Google Update
// during app install.
bool ReadGoogleUpdateStrKey(const wchar_t* const name, std::wstring* value) {
  return ReadGoogleUpdateStrKeyFromRoot(install_static::IsSystemInstall()
                                            ? HKEY_LOCAL_MACHINE
                                            : HKEY_CURRENT_USER,
                                        name, value);
}

// Reads the value |name| from the app's ClientState/cohort registry key.
std::optional<std::wstring> ReadGoogleUpdateCohortStrKey(
    const wchar_t* const name) {
  return ReadGoogleUpdateCohortStrKeyFromRoot(install_static::IsSystemInstall()
                                                  ? HKEY_LOCAL_MACHINE
                                                  : HKEY_CURRENT_USER,
                                              name);
}

// Writes |value| into |name| in the app's ClientState key in HKEY_CURRENT_USER.
// This function is only provided for legacy use. New code needing to load/store
// per-user data should use install_details::GetRegistryPath().
bool WriteUserGoogleUpdateStrKey(const wchar_t* const name,
                                 const std::wstring& value) {
  RegKey key;
  return key.Create(HKEY_CURRENT_USER,
                    install_static::GetClientStateKeyPath().c_str(),
                    KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
         key.WriteValue(name, value.c_str()) == ERROR_SUCCESS;
}

// Clears |name| in the app's ClientState key by writing an empty string into
// it. Returns true if the value does not exist, is already clear, or is
// successfully cleared.
bool ClearGoogleUpdateStrKey(const wchar_t* const name) {
  RegKey key;
  auto result = key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                           : HKEY_CURRENT_USER,
                         install_static::GetClientStateKeyPath().c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_PATH_NOT_FOUND || result == ERROR_FILE_NOT_FOUND)
    return true;  // The key doesn't exist; consider the value cleared.
  if (result != ERROR_SUCCESS)
    return false;  // Failed to open the key.

  std::wstring value;
  result = key.ReadValue(name, &value);
  if (result == ERROR_FILE_NOT_FOUND ||
      (result == ERROR_SUCCESS && value.empty())) {
    return true;  // The value doesn't exist or is empty; consider it cleared.
  }
  return key.WriteValue(name, L"") == ERROR_SUCCESS;
}

// Deletes the value |name| from the app's ClientState key in HKEY_CURRENT_USER.
// Returns true if the value does not exist or is successfully deleted.
bool RemoveUserGoogleUpdateStrKey(const wchar_t* const name) {
  RegKey key;
  auto result = key.Open(HKEY_CURRENT_USER,
                         install_static::GetClientStateKeyPath().c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_PATH_NOT_FOUND || result == ERROR_FILE_NOT_FOUND)
    return true;  // The key doesn't exist; consider the value cleared.
  if (result != ERROR_SUCCESS)
    return false;  // Failed to open the key.

  std::wstring value;
  return key.ReadValue(name, &value) == ERROR_FILE_NOT_FOUND ||
         key.DeleteValue(name) == ERROR_SUCCESS;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Populates |update_policy| with the UpdatePolicy enum value corresponding to a
// DWORD read from the registry and returns true if |value| is within range.
// If |value| is out of range, returns false without modifying |update_policy|.
bool GetUpdatePolicyFromDword(
    const DWORD value,
    GoogleUpdateSettings::UpdatePolicy* update_policy) {
  switch (value) {
    case GoogleUpdateSettings::UPDATES_DISABLED:
    case GoogleUpdateSettings::AUTOMATIC_UPDATES:
    case GoogleUpdateSettings::MANUAL_UPDATES_ONLY:
    case GoogleUpdateSettings::AUTO_UPDATES_ONLY:
      *update_policy = static_cast<GoogleUpdateSettings::UpdatePolicy>(value);
      return true;
    default:
      LOG(WARNING) << "Unexpected update policy override value: " << value;
  }
  return false;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Returns the stats consent tristate held in the registry keys given by the two
// functions |state_key_fn_ptr| and |state_medium_key_fn_ptr|. The state is read
// from the ClientState key in HKCU for user-level installs, and from either the
// ClientStateMedium key or the ClientState key in HKLM for system-level
// installs.
google_update::Tristate GetCollectStatsConsentImpl(
    decltype(&install_static::GetClientStateKeyPath) state_key_fn_ptr,
    decltype(
        &install_static::GetClientStateMediumKeyPath) state_medium_key_fn_ptr) {
  const bool system_install = install_static::IsSystemInstall();
  DWORD value = google_update::TRISTATE_NONE;
  bool have_value = false;
  RegKey key;
  const REGSAM kAccess = KEY_QUERY_VALUE | KEY_WOW64_32KEY;

  // For system-level installs, try ClientStateMedium first.
  have_value = system_install &&
               key.Open(HKEY_LOCAL_MACHINE, state_medium_key_fn_ptr().c_str(),
                        kAccess) == ERROR_SUCCESS &&
               key.ReadValueDW(google_update::kRegUsageStatsField, &value) ==
                   ERROR_SUCCESS;

  // Otherwise, try ClientState.
  if (!have_value) {
    have_value =
        key.Open(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                 state_key_fn_ptr().c_str(), kAccess) == ERROR_SUCCESS &&
        key.ReadValueDW(google_update::kRegUsageStatsField, &value) ==
            ERROR_SUCCESS;
  }

  if (!have_value)
    return google_update::TRISTATE_NONE;

  return value == google_update::TRISTATE_TRUE ? google_update::TRISTATE_TRUE
                                               : google_update::TRISTATE_FALSE;
}

}  // namespace

// TODO(grt): Remove this now that it has no added value.
bool GoogleUpdateSettings::IsSystemInstall() {
  return !InstallUtil::IsPerUserInstall();
}

base::SequencedTaskRunner*
GoogleUpdateSettings::CollectStatsConsentTaskRunner() {
  // TODO(fdoray): Use LazyThreadPoolSequencedTaskRunner::GetRaw() here instead
  // of .Get().get() when it's added to the API, http://crbug.com/730170.
  return g_collect_stats_consent_task_runner.Get().get();
}

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  return GetCollectStatsConsentImpl(
             &install_static::GetClientStateKeyPath,
             &install_static::GetClientStateMediumKeyPath) ==
         google_update::TRISTATE_TRUE;
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  DWORD value =
      consented ? google_update::TRISTATE_TRUE : google_update::TRISTATE_FALSE;

  // Write to ClientStateMedium for system-level; ClientState otherwise.
  const bool system_install = install_static::IsSystemInstall();
  HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring reg_path = system_install
                              ? install_static::GetClientStateMediumKeyPath()
                              : install_static::GetClientStateKeyPath();
  RegKey key;
  LONG result =
      key.Create(root_key, reg_path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed opening key " << reg_path << " to set "
               << google_update::kRegUsageStatsField << "; result: " << result;
  } else {
    result = key.WriteValue(google_update::kRegUsageStatsField, value);
    LOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed setting " << google_update::kRegUsageStatsField << " in key "
        << reg_path << "; result: " << result;
  }

  // When opting out, clear registry backup of client id and related values.
  if (result == ERROR_SUCCESS && !consented)
    StoreMetricsClientInfo(metrics::ClientInfo());

  return (result == ERROR_SUCCESS);
}

// static
bool GoogleUpdateSettings::GetCollectStatsConsentDefault(
    bool* stats_consent_default) {
  wchar_t stats_default = installer::AdditionalParameters().GetStatsDefault();
  if (stats_default == L'0') {
    *stats_consent_default = false;
    return true;
  }
  if (stats_default == L'1') {
    *stats_consent_default = true;
    return true;
  }
  return false;
}

std::unique_ptr<metrics::ClientInfo>
GoogleUpdateSettings::LoadMetricsClientInfo() {
  std::wstring client_id_16;
  if (!ReadUserGoogleUpdateStrKey(google_update::kRegMetricsId,
                                  &client_id_16) ||
      client_id_16.empty()) {
    return nullptr;
  }

  std::unique_ptr<metrics::ClientInfo> client_info(new metrics::ClientInfo);
  client_info->client_id = base::WideToUTF8(client_id_16);

  std::wstring installation_date_str;
  if (ReadUserGoogleUpdateStrKey(google_update::kRegMetricsIdInstallDate,
                                 &installation_date_str)) {
    base::StringToInt64(installation_date_str, &client_info->installation_date);
  }

  std::wstring reporting_enbaled_date_date_str;
  if (ReadUserGoogleUpdateStrKey(google_update::kRegMetricsIdEnabledDate,
                                 &reporting_enbaled_date_date_str)) {
    base::StringToInt64(reporting_enbaled_date_date_str,
                        &client_info->reporting_enabled_date);
  }

  return client_info;
}

// static
std::optional<uint32_t> GoogleUpdateSettings::GetHashedCohortId() {
  std::optional<std::wstring> id =
      ReadGoogleUpdateCohortStrKey(google_update::kRegDefaultField);
  if (!id) {
    return std::nullopt;
  }
  std::string id_utf8 = base::WideToUTF8(*id);
  // Duplicate the logic of
  // ComponentMetricsProvider::ProvideSystemProfileMetrics: For component_id
  // strings in the "1:A:B" format, ignore the B segment; including it will
  // result in two clients assigned to the same cohort lineage (A) hashing to
  // different values. (The B segment tracks data about the size of fractional
  // cohorts that do not contain this client.)
  size_t last_colon = id_utf8.find_last_of(":");
  if (last_colon == std::string::npos) {
    // No colon separator indicates some unexpected id format; abandon trying
    // to interpret it.
    return std::nullopt;
  }
  return base::PersistentHash(std::string_view(id_utf8.c_str(), last_colon));
}

void GoogleUpdateSettings::StoreMetricsClientInfo(
    const metrics::ClientInfo& client_info) {
  // Attempt a best-effort at backing |client_info| in the registry (but don't
  // handle/report failures).
  WriteUserGoogleUpdateStrKey(google_update::kRegMetricsId,
                              base::UTF8ToWide(client_info.client_id));
  WriteUserGoogleUpdateStrKey(
      google_update::kRegMetricsIdInstallDate,
      base::NumberToWString(client_info.installation_date));
  WriteUserGoogleUpdateStrKey(
      google_update::kRegMetricsIdEnabledDate,
      base::NumberToWString(client_info.reporting_enabled_date));
}

// EULA consent is only relevant for system-level installs.
bool GoogleUpdateSettings::SetEulaConsent(
    const InstallationState& machine_state,
    bool consented) {
  const DWORD eula_accepted = consented ? 1 : 0;
  const REGSAM kAccess = KEY_SET_VALUE | KEY_WOW64_32KEY;
  RegKey key;

  // Write the consent value into the product's ClientStateMedium key.
  return key.Create(HKEY_LOCAL_MACHINE,
                    install_static::GetClientStateMediumKeyPath().c_str(),
                    kAccess) == ERROR_SUCCESS &&
         key.WriteValue(google_update::kRegEulaAceptedField, eula_accepted) ==
             ERROR_SUCCESS;
}

int GoogleUpdateSettings::GetLastRunTime() {
  std::wstring time_s;
  if (!ReadUserGoogleUpdateStrKey(google_update::kRegLastRunTimeField, &time_s))
    return -1;
  int64_t time_i;
  if (!base::StringToInt64(time_s, &time_i))
    return -1;
  base::TimeDelta td =
      base::Time::NowFromSystemTime() - base::Time::FromInternalValue(time_i);
  return td.InDays();
}

bool GoogleUpdateSettings::SetLastRunTime() {
  int64_t time = base::Time::NowFromSystemTime().ToInternalValue();
  return WriteUserGoogleUpdateStrKey(google_update::kRegLastRunTimeField,
                                     base::NumberToWString(time));
}

bool GoogleUpdateSettings::RemoveLastRunTime() {
  return RemoveUserGoogleUpdateStrKey(google_update::kRegLastRunTimeField);
}

bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  // Written by Google Update.
  return ReadGoogleUpdateStrKey(google_update::kRegBrowserField, browser);
}

bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  // Written by Google Update.
  return ReadGoogleUpdateStrKey(google_update::kRegLangField, language);
}

bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  // Written by Google Update.
  return ReadGoogleUpdateStrKey(google_update::kRegRLZBrandField, brand);
}

bool GoogleUpdateSettings::GetReactivationBrand(std::wstring* brand) {
  // Written by GCAPI into HKCU.
  return ReadUserGoogleUpdateStrKey(
      google_update::kRegRLZReactivationBrandField, brand);
}

bool GoogleUpdateSettings::GetReferral(std::wstring* referral) {
  // Written by Google Update.
  return ReadGoogleUpdateStrKey(google_update::kRegReferralField, referral);
}

bool GoogleUpdateSettings::ClearReferral() {
  // This is expected to fail when called from within the browser for
  // system-level installs, as the user generally does not have the required
  // rights to modify keys in HKLM.
  return ClearGoogleUpdateStrKey(google_update::kRegReferralField);
}

void GoogleUpdateSettings::UpdateInstallStatus(
    bool system_install,
    installer::ArchiveType archive_type,
    int install_return_code) {
  DCHECK(archive_type != installer::UNKNOWN_ARCHIVE_TYPE ||
         install_return_code != 0);

  installer::AdditionalParameters additional_parameters;
  if (UpdateGoogleUpdateApKey(archive_type, install_return_code,
                              &additional_parameters) &&
      !additional_parameters.Commit()) {
    PLOG(ERROR) << "Failed to write to application's ClientState key "
                << google_update::kRegApField << " = "
                << additional_parameters.value();
  }
}

void GoogleUpdateSettings::SetProgress(bool system_install,
                                       const std::wstring& path,
                                       int progress) {
  DCHECK_GE(progress, 0);
  DCHECK_LE(progress, 100);
  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey key(root, path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (key.Valid()) {
    key.WriteValue(google_update::kRegInstallerProgress,
                   static_cast<DWORD>(progress));
  }
}

bool GoogleUpdateSettings::UpdateGoogleUpdateApKey(
    installer::ArchiveType archive_type,
    int install_return_code,
    installer::AdditionalParameters* additional_parameters) {
  DCHECK(archive_type != installer::UNKNOWN_ARCHIVE_TYPE ||
         install_return_code != 0);
  bool modified = false;

  if (archive_type == installer::FULL_ARCHIVE_TYPE || !install_return_code) {
    if (additional_parameters->SetFullSuffix(false)) {
      VLOG(1) << "Removed incremental installer failure key; "
                 "switching to channel: "
              << additional_parameters->value();
      modified = true;
    }
  } else if (archive_type == installer::INCREMENTAL_ARCHIVE_TYPE) {
    if (additional_parameters->SetFullSuffix(true)) {
      VLOG(1) << "Incremental installer failed; switching to channel: "
              << additional_parameters->value();
      modified = true;
    } else {
      VLOG(1) << "Incremental installer failure; already on channel: "
              << additional_parameters->value();
    }
  } else {
    // It's okay if we don't know the archive type.  In this case, leave the
    // "-full" suffix as we found it.
    DCHECK_EQ(installer::UNKNOWN_ARCHIVE_TYPE, archive_type);
  }

  return modified;
}

GoogleUpdateSettings::UpdatePolicy GoogleUpdateSettings::GetAppUpdatePolicy(
    std::wstring_view app_guid,
    bool* is_overridden) {
  bool found_override = false;
  UpdatePolicy update_policy = kDefaultUpdatePolicy;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  DCHECK(!app_guid.empty());
  RegKey policy_key;

  // Google Update Group Policy settings are always in HKLM.
  // TODO(wfh): Check if policies should go into Wow6432Node or not.
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey, KEY_QUERY_VALUE) ==
      ERROR_SUCCESS) {
    DWORD value = 0;
    std::wstring app_update_override =
        base::StrCat({kUpdateOverrideValuePrefix, app_guid});
    // First try to read and comprehend the app-specific override.
    found_override = (policy_key.ReadValueDW(app_update_override.c_str(),
                                             &value) == ERROR_SUCCESS &&
                      GetUpdatePolicyFromDword(value, &update_policy));

    // Failing that, try to read and comprehend the default override.
    if (!found_override &&
        policy_key.ReadValueDW(kUpdatePolicyValue, &value) == ERROR_SUCCESS) {
      GetUpdatePolicyFromDword(value, &update_policy);
    }
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  if (is_overridden != nullptr)
    *is_overridden = found_override;

  return update_policy;
}

// static
bool GoogleUpdateSettings::AreAutoupdatesEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Check the auto-update check period override. If it is 0 or exceeds the
  // maximum timeout, then for all intents and purposes auto updates are
  // disabled.
  RegKey policy_key;
  DWORD value = 0;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey, KEY_QUERY_VALUE) ==
          ERROR_SUCCESS &&
      policy_key.ReadValueDW(kCheckPeriodOverrideMinutes, &value) ==
          ERROR_SUCCESS &&
      (value == 0 || value > kCheckPeriodOverrideMinutesMax)) {
    return false;
  }

  UpdatePolicy app_policy =
      GetAppUpdatePolicy(install_static::GetAppGuid(), nullptr);
  return app_policy == AUTOMATIC_UPDATES || app_policy == AUTO_UPDATES_ONLY;
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Chromium does not auto update.
  return false;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
bool GoogleUpdateSettings::ReenableAutoupdates() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  int needs_reset_count = 0;
  int did_reset_count = 0;

  UpdatePolicy update_policy = kDefaultUpdatePolicy;
  RegKey policy_key;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey,
                      KEY_SET_VALUE | KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    // Set to true while app-specific overrides are present that allow automatic
    // updates. When this is the case, the defaults are irrelevant and don't
    // need to be checked or reset.
    bool automatic_updates_allowed_by_overrides = true;
    DWORD value = 0;

    // First check the app-specific override value and reset that if needed.
    // Note that this intentionally sets the override to AUTOMATIC_UPDATES even
    // if it was previously AUTO_UPDATES_ONLY. The thinking is that
    // AUTOMATIC_UPDATES is marginally more likely to let a user update and this
    // code is only called when a stuck user asks for updates.
    std::wstring app_update_override(kUpdateOverrideValuePrefix);
    app_update_override.append(install_static::GetAppGuid());
    if (policy_key.ReadValueDW(app_update_override.c_str(), &value) !=
        ERROR_SUCCESS) {
      automatic_updates_allowed_by_overrides = false;
    } else if (!GetUpdatePolicyFromDword(value, &update_policy) ||
               update_policy != GoogleUpdateSettings::AUTOMATIC_UPDATES) {
      automatic_updates_allowed_by_overrides = false;
      ++needs_reset_count;
      if (policy_key.WriteValue(
              app_update_override.c_str(),
              static_cast<DWORD>(GoogleUpdateSettings::AUTOMATIC_UPDATES)) ==
          ERROR_SUCCESS) {
        ++did_reset_count;
      }
    }

    // If there were no app-specific override policies, see if there's a global
    // policy preventing updates and delete it if so.
    if (!automatic_updates_allowed_by_overrides &&
        policy_key.ReadValueDW(kUpdatePolicyValue, &value) == ERROR_SUCCESS &&
        (!GetUpdatePolicyFromDword(value, &update_policy) ||
         update_policy != GoogleUpdateSettings::AUTOMATIC_UPDATES)) {
      ++needs_reset_count;
      if (policy_key.DeleteValue(kUpdatePolicyValue) == ERROR_SUCCESS)
        ++did_reset_count;
    }

    // Check the auto-update check period override. If it is 0 or exceeds
    // the maximum timeout, delete the override value.
    if (policy_key.ReadValueDW(kCheckPeriodOverrideMinutes, &value) ==
            ERROR_SUCCESS &&
        (value == 0 || value > kCheckPeriodOverrideMinutesMax)) {
      ++needs_reset_count;
      if (policy_key.DeleteValue(kCheckPeriodOverrideMinutes) == ERROR_SUCCESS)
        ++did_reset_count;
    }

    // Return whether the number of successful resets is the same as the
    // number of things that appeared to need resetting.
    return (needs_reset_count == did_reset_count);
  } else {
    // For some reason we couldn't open the policy key with the desired
    // permissions to make changes (the most likely reason is that there is no
    // policy set). Simply return whether or not we think updates are enabled.
    return AreAutoupdatesEnabled();
  }
#else
  // Non Google Chrome isn't going to autoupdate.
  return true;
#endif
}

// Reads and sanitizes the value of
// "HKLM\SOFTWARE\Policies\Google\Update\DownloadPreference". A valid
// group policy option must be a single alpha numeric word of up to 32
// characters.
std::wstring GoogleUpdateSettings::GetDownloadPreference() {
  RegKey policy_key;
  std::wstring value;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey, KEY_QUERY_VALUE) ==
          ERROR_SUCCESS &&
      policy_key.ReadValue(kDownloadPreferencePolicyValue, &value) ==
          ERROR_SUCCESS) {
    // Validates that |value| matches `[a-zA-z]{0-32}`.
    const size_t kMaxValueLength = 32;
    if (value.size() > kMaxValueLength)
      return std::wstring();
    for (auto ch : value) {
      if (!base::IsAsciiAlpha(ch))
        return std::wstring();
    }
    return value;
  }
  return std::wstring();
}

std::wstring GoogleUpdateSettings::GetUninstallCommandLine(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring cmd_line;
  RegKey update_key;

  if (update_key.Open(root_key, google_update::kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    update_key.ReadValue(google_update::kRegUninstallCmdLine, &cmd_line);
  }

  return cmd_line;
}

base::Version GoogleUpdateSettings::GetGoogleUpdateVersion(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring version;
  RegKey key;

  if (key.Open(root_key, google_update::kRegPathGoogleUpdate,
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegGoogleUpdateVersion, &version) ==
          ERROR_SUCCESS) {
    return base::Version(base::WideToASCII(version));
  }

  return base::Version();
}

base::Time GoogleUpdateSettings::GetGoogleUpdateLastStartedAU(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey update_key;

  if (update_key.Open(root_key, google_update::kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD last_start;
    if (update_key.ReadValueDW(google_update::kRegLastStartedAUField,
                               &last_start) == ERROR_SUCCESS) {
      return base::Time::FromTimeT(last_start);
    }
  }

  return base::Time();
}

base::Time GoogleUpdateSettings::GetGoogleUpdateLastChecked(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey update_key;

  if (update_key.Open(root_key, google_update::kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD last_check;
    if (update_key.ReadValueDW(google_update::kRegLastCheckedField,
                               &last_check) == ERROR_SUCCESS) {
      return base::Time::FromTimeT(last_check);
    }
  }

  return base::Time();
}

bool GoogleUpdateSettings::GetUpdateDetailForApp(bool system_install,
                                                 const wchar_t* app_guid,
                                                 ProductData* data) {
  DCHECK(app_guid);
  DCHECK(data);

  bool product_found = false;

  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring clientstate_reg_path(google_update::kRegPathClientState);
  clientstate_reg_path.append(L"\\");
  clientstate_reg_path.append(app_guid);

  RegKey clientstate;
  if (clientstate.Open(root_key, clientstate_reg_path.c_str(),
                       KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    std::wstring version;
    DWORD dword_value;
    if ((clientstate.ReadValueDW(google_update::kRegLastCheckSuccessField,
                                 &dword_value) == ERROR_SUCCESS) &&
        (clientstate.ReadValue(google_update::kRegVersionField, &version) ==
         ERROR_SUCCESS)) {
      product_found = true;
      data->version = base::WideToASCII(version);
      data->last_success = base::Time::FromTimeT(dword_value);
      data->last_result = 0;
      data->last_error_code = 0;
      data->last_extra_code = 0;

      if (clientstate.ReadValueDW(google_update::kRegLastInstallerResultField,
                                  &dword_value) == ERROR_SUCCESS) {
        // Google Update convention is that if an installer writes an result
        // code that is invalid, it is clamped to an exit code result.
        const DWORD kMaxValidInstallResult = 4;  // INSTALLER_RESULT_EXIT_CODE
        data->last_result = std::min(dword_value, kMaxValidInstallResult);
      }
      if (clientstate.ReadValueDW(google_update::kRegLastInstallerErrorField,
                                  &dword_value) == ERROR_SUCCESS) {
        data->last_error_code = dword_value;
      }
      if (clientstate.ReadValueDW(google_update::kRegLastInstallerExtraField,
                                  &dword_value) == ERROR_SUCCESS) {
        data->last_extra_code = dword_value;
      }
    }
  }

  return product_found;
}

bool GoogleUpdateSettings::GetUpdateDetailForGoogleUpdate(ProductData* data) {
  return GetUpdateDetailForApp(!InstallUtil::IsPerUserInstall(),
                               google_update::kGoogleUpdateUpgradeCode, data);
}

bool GoogleUpdateSettings::GetUpdateDetail(ProductData* data) {
  return GetUpdateDetailForApp(!InstallUtil::IsPerUserInstall(),
                               install_static::GetAppGuid(), data);
}
