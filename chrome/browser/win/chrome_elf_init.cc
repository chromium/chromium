// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_elf_init.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/win/registry.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/dll_hash/dll_hash.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "services/service_manager/sandbox/features.h"

const char kBrowserBlacklistTrialName[] = "BrowserBlacklist";
const char kBrowserBlacklistTrialDisabledGroupName[] = "NoBlacklist";

namespace {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended in front of
//       BLACKLIST_SETUP_EVENT_MAX.
enum BlacklistSetupEventType {
  // The blacklist beacon has placed to enable the browser blacklisting.
  BLACKLIST_SETUP_ENABLED = 0,

  // The blacklist was successfully enabled.
  BLACKLIST_SETUP_RAN_SUCCESSFULLY,

  // The blacklist setup code failed to execute.
  BLACKLIST_SETUP_FAILED,

  // The blacklist thunk setup code failed. This is probably an indication
  // that something else patched that code first.
  BLACKLIST_THUNK_SETUP_FAILED,

  // Deprecated. The blacklist interception code failed to execute.
  BLACKLIST_INTERCEPTION_FAILED,

  // The blacklist was disabled for this run (after it failed too many times).
  BLACKLIST_SETUP_DISABLED,

  // Always keep this at the end.
  BLACKLIST_SETUP_EVENT_MAX,
};

void RecordBlacklistSetupEvent(BlacklistSetupEventType blacklist_setup_event) {
  base::UmaHistogramEnumeration("ChromeElf.Beacon.SetupStatus",
                                blacklist_setup_event,
                                BLACKLIST_SETUP_EVENT_MAX);
}

base::string16 GetBeaconRegistryPath() {
  return install_static::GetRegistryPath().append(
      blacklist::kRegistryBeaconKeyName);
}

}  // namespace

void InitializeChromeElf() {
  if (base::FieldTrialList::FindFullName(kBrowserBlacklistTrialName) ==
      kBrowserBlacklistTrialDisabledGroupName) {
    // Disable the blacklist for all future runs by removing the beacon.
    base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER);
    blacklist_registry_key.DeleteKey(GetBeaconRegistryPath().c_str());
  } else {
    BrowserBlacklistBeaconSetup();
  }

  // Make sure the early finch emergency "off switch" for
  // sandbox::MITIGATION_EXTENSION_POINT_DISABLE is set properly in reg.
  // Note: the very existence of this key signals elf to not enable
  // this mitigation on browser next start.
  const base::string16 finch_path(install_static::GetRegistryPath().append(
      elf_sec::kRegSecurityFinchKeyName));
  base::win::RegKey finch_security_registry_key(HKEY_CURRENT_USER,
                                                finch_path.c_str(), KEY_READ);

  if (base::FeatureList::IsEnabled(
          service_manager::features::kWinSboxDisableExtensionPoints)) {
    if (finch_security_registry_key.Valid())
      finch_security_registry_key.DeleteKey(L"");
  } else {
    if (!finch_security_registry_key.Valid()) {
      finch_security_registry_key.Create(HKEY_CURRENT_USER, finch_path.c_str(),
                                         KEY_WRITE);
    }
  }
}

void BrowserBlacklistBeaconSetup() {
  base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER,
                                           GetBeaconRegistryPath().c_str(),
                                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  // No point in trying to continue if the registry key isn't valid.
  if (!blacklist_registry_key.Valid())
    return;

  // Record the results of the last blacklist setup.
  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  blacklist_registry_key.ReadValueDW(blacklist::kBeaconState, &blacklist_state);

  if (blacklist_state == blacklist::BLACKLIST_ENABLED) {
    // The blacklist setup didn't crash, so we report if it was enabled or not.
    if (IsThirdPartyInitialized()) {
      RecordBlacklistSetupEvent(BLACKLIST_SETUP_RAN_SUCCESSFULLY);
    } else {
      // The only way for the blacklist to be enabled, but not fully
      // initialized is if the thunk setup failed. See blacklist.cc
      // for more details.
      RecordBlacklistSetupEvent(BLACKLIST_THUNK_SETUP_FAILED);
    }

    // Regardless of if the blacklist was fully enabled or not, report how many
    // times we had to try to set it up.
    DWORD attempt_count = 0;
    blacklist_registry_key.ReadValueDW(blacklist::kBeaconAttemptCount,
                                       &attempt_count);
    base::UmaHistogramCounts100("ChromeElf.Beacon.RetryAttemptsBeforeSuccess",
                                attempt_count);
  } else if (blacklist_state == blacklist::BLACKLIST_SETUP_FAILED) {
    // We can set the state to disabled without checking that the maximum number
    // of attempts was exceeded because blacklist.cc has already done this.
    RecordBlacklistSetupEvent(BLACKLIST_SETUP_FAILED);
    blacklist_registry_key.WriteValue(blacklist::kBeaconState,
                                      blacklist::BLACKLIST_DISABLED);
  } else if (blacklist_state == blacklist::BLACKLIST_DISABLED) {
    RecordBlacklistSetupEvent(BLACKLIST_SETUP_DISABLED);
  }

  // Find the last recorded blacklist version.
  base::string16 blacklist_version;
  blacklist_registry_key.ReadValue(blacklist::kBeaconVersion,
                                   &blacklist_version);

  if (blacklist_version != TEXT(CHROME_VERSION_STRING)) {
    // The blacklist hasn't been enabled for this version yet, so enable it
    // and reset the failure count to zero.
    LONG set_version = blacklist_registry_key.WriteValue(
        blacklist::kBeaconVersion,
        TEXT(CHROME_VERSION_STRING));

    LONG set_state = blacklist_registry_key.WriteValue(
        blacklist::kBeaconState,
        blacklist::BLACKLIST_ENABLED);

    blacklist_registry_key.WriteValue(blacklist::kBeaconAttemptCount,
                                      static_cast<DWORD>(0));

    // Only report the blacklist as getting setup when both registry writes
    // succeed, since otherwise the blacklist wasn't properly setup.
    if (set_version == ERROR_SUCCESS && set_state == ERROR_SUCCESS)
      RecordBlacklistSetupEvent(BLACKLIST_SETUP_ENABLED);
  }
}
