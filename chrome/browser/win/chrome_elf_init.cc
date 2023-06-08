// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_elf_init.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/chrome_elf/blocklist_constants.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/dll_hash/dll_hash.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/install_static/install_util.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "sandbox/policy/features.h"

const char kBrowserBlocklistTrialName[] = "BrowserBlocklist";
const char kBrowserBlocklistTrialDisabledGroupName[] = "NoBlocklist";

namespace {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended in front of
//       BLOCKLIST_SETUP_EVENT_MAX.
enum BlocklistSetupEventType {
  // The blocklist beacon has placed to enable the browser blocklisting.
  BLOCKLIST_SETUP_ENABLED = 0,

  // The blocklist was successfully enabled.
  BLOCKLIST_SETUP_RAN_SUCCESSFULLY,

  // The blocklist setup code failed to execute.
  BLOCKLIST_SETUP_FAILED,

  // The blocklist thunk setup code failed. This is probably an indication
  // that something else patched that code first.
  BLOCKLIST_THUNK_SETUP_FAILED,

  // Deprecated. The blocklist interception code failed to execute.
  BLOCKLIST_INTERCEPTION_FAILED,

  // The blocklist was disabled for this run (after it failed too many times).
  BLOCKLIST_SETUP_DISABLED,

  // Always keep this at the end.
  BLOCKLIST_SETUP_EVENT_MAX,
};

void RecordBlocklistSetupEvent(BlocklistSetupEventType blocklist_setup_event) {
  base::UmaHistogramEnumeration("ChromeElf.Beacon.SetupStatus",
                                blocklist_setup_event,
                                BLOCKLIST_SETUP_EVENT_MAX);
}

std::wstring GetBeaconRegistryPath() {
  return install_static::GetRegistryPath().append(
      blocklist::kRegistryBeaconKeyName);
}

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended in front of EXTENSIONPOINT_MAX.
enum ExtensionPointEnableState {
  // Extension point mitigation disabled due to presence of legacy IME.
  EXTENSIONPOINT_DISABLED_IME,

  // Extension point mitigation enabled.
  EXTENSIONPOINT_ENABLED,

  // Always keep this at the end.
  EXTENSIONPOINT_MAX,
};

void RecordExtensionPointsEnableState(ExtensionPointEnableState enable_state) {
  base::UmaHistogramEnumeration("ChromeElf.ExtensionPoint.EnableState",
                                enable_state, EXTENSIONPOINT_MAX);
}

ExtensionPointEnableState GetExtensionPointsEnableState() {
  // Legacy IMEs can be detected as HKLs that have a file name.
  int list_size = GetKeyboardLayoutList(0, nullptr);
  if (list_size != 0) {
    std::vector<HKL> hkl_list(list_size);
    if (GetKeyboardLayoutList(list_size, hkl_list.data()) == list_size) {
      for (auto* hkl : hkl_list) {
        if (ImmGetIMEFileName(hkl, nullptr, 0) != 0)
          return EXTENSIONPOINT_DISABLED_IME;
      }
    }
  }
  return EXTENSIONPOINT_ENABLED;
}

bool IsBrowserLegacyExtensionPointsBlocked() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state ||
      !local_state->HasPrefPath(prefs::kBlockBrowserLegacyExtensionPoints) ||
      !local_state->IsManagedPreference(
          prefs::kBlockBrowserLegacyExtensionPoints))
    return true;
  return local_state->GetBoolean(prefs::kBlockBrowserLegacyExtensionPoints);
}

}  // namespace

void InitializeChromeElf() {
  if (base::FieldTrialList::FindFullName(kBrowserBlocklistTrialName) ==
      kBrowserBlocklistTrialDisabledGroupName) {
    // Disable the blocklist for all future runs by removing the beacon.
    base::win::RegKey blocklist_registry_key(HKEY_CURRENT_USER);
    blocklist_registry_key.DeleteKey(GetBeaconRegistryPath().c_str());
  } else {
    BrowserBlocklistBeaconSetup();
  }

  // Make sure the registry key we read earlier in startup
  // sandbox::MITIGATION_EXTENSION_POINT_DISABLE is set properly in reg.
  // Note: the very existence of this key signals elf to not enable
  // this mitigation on browser next start.
  const std::wstring reg_path(install_static::GetRegistryPath().append(
      elf_sec::kRegBrowserExtensionPointKeyName));
  base::win::RegKey browser_extension_point_registry_key(
      HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);

  ExtensionPointEnableState extension_point_enable_state =
      GetExtensionPointsEnableState();
  RecordExtensionPointsEnableState(extension_point_enable_state);
  bool enable_extension_point_policy =
      (extension_point_enable_state == EXTENSIONPOINT_ENABLED) &&
      base::FeatureList::IsEnabled(
          sandbox::policy::features::kWinSboxDisableExtensionPoints) &&
      IsBrowserLegacyExtensionPointsBlocked();

  if (enable_extension_point_policy) {
    if (!browser_extension_point_registry_key.Valid()) {
      (void)browser_extension_point_registry_key.Create(
          HKEY_CURRENT_USER, reg_path.c_str(), KEY_WRITE);
    }
  } else {
    if (browser_extension_point_registry_key.Valid()) {
      browser_extension_point_registry_key.DeleteKey(L"");
    }
  }
}

void BrowserBlocklistBeaconSetup() {
  base::win::RegKey blocklist_registry_key(HKEY_CURRENT_USER,
                                           GetBeaconRegistryPath().c_str(),
                                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  // No point in trying to continue if the registry key isn't valid.
  if (!blocklist_registry_key.Valid())
    return;

  // Record the results of the last blocklist setup.
  DWORD blocklist_state = blocklist::BLOCKLIST_STATE_MAX;
  blocklist_registry_key.ReadValueDW(blocklist::kBeaconState, &blocklist_state);

  if (blocklist_state == blocklist::BLOCKLIST_ENABLED) {
    // The blocklist setup didn't crash, so we report if it was enabled or not.
    if (IsThirdPartyInitialized()) {
      RecordBlocklistSetupEvent(BLOCKLIST_SETUP_RAN_SUCCESSFULLY);
    } else {
      // The only way for the blocklist to be enabled, but not fully
      // initialized is if the thunk setup failed. See blocklist.cc
      // for more details.
      RecordBlocklistSetupEvent(BLOCKLIST_THUNK_SETUP_FAILED);
    }

    // Regardless of if the blocklist was fully enabled or not, report how many
    // times we had to try to set it up.
    DWORD attempt_count = 0;
    blocklist_registry_key.ReadValueDW(blocklist::kBeaconAttemptCount,
                                       &attempt_count);
    base::UmaHistogramCounts100("ChromeElf.Beacon.RetryAttemptsBeforeSuccess",
                                attempt_count);
  } else if (blocklist_state == blocklist::BLOCKLIST_SETUP_FAILED) {
    // We can set the state to disabled without checking that the maximum number
    // of attempts was exceeded because blocklist.cc has already done this.
    RecordBlocklistSetupEvent(BLOCKLIST_SETUP_FAILED);
    blocklist_registry_key.WriteValue(blocklist::kBeaconState,
                                      blocklist::BLOCKLIST_DISABLED);
  } else if (blocklist_state == blocklist::BLOCKLIST_DISABLED) {
    RecordBlocklistSetupEvent(BLOCKLIST_SETUP_DISABLED);
  }

  // Find the last recorded blocklist version.
  std::wstring blocklist_version;
  blocklist_registry_key.ReadValue(blocklist::kBeaconVersion,
                                   &blocklist_version);

  if (blocklist_version != TEXT(CHROME_VERSION_STRING)) {
    // The blocklist hasn't been enabled for this version yet, so enable it
    // and reset the failure count to zero.
    LONG set_version = blocklist_registry_key.WriteValue(
        blocklist::kBeaconVersion,
        TEXT(CHROME_VERSION_STRING));

    LONG set_state = blocklist_registry_key.WriteValue(
        blocklist::kBeaconState,
        blocklist::BLOCKLIST_ENABLED);

    blocklist_registry_key.WriteValue(blocklist::kBeaconAttemptCount,
                                      static_cast<DWORD>(0));

    // Only report the blocklist as getting setup when both registry writes
    // succeed, since otherwise the blocklist wasn't properly setup.
    if (set_version == ERROR_SUCCESS && set_state == ERROR_SUCCESS)
      RecordBlocklistSetupEvent(BLOCKLIST_SETUP_ENABLED);
  }
}
