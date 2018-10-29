// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_elf_init.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/dll_hash/dll_hash.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "services/service_manager/sandbox/features.h"

const char kBrowserBlacklistTrialName[] = "BrowserBlacklist";
const char kBrowserBlacklistTrialDisabledGroupName[] = "NoBlacklist";

namespace {

// How long to wait, in seconds, before reporting for the second (and last
// time), what dlls were blocked from the browser process.
const int kBlacklistReportingDelaySec = 600;

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
  UMA_HISTOGRAM_ENUMERATION("Blacklist.Setup",
                            blacklist_setup_event,
                            BLACKLIST_SETUP_EVENT_MAX);
}

// Report which DLLs were prevented from being loaded.
void ReportSuccessfulBlocks() {
  // Figure out how many dlls were blocked.
  int num_blocked_dlls = 0;
  blacklist::SuccessfullyBlocked(NULL, &num_blocked_dlls);

  if (num_blocked_dlls == 0)
    return;

  // Now retrieve the list of blocked dlls.
  std::vector<const wchar_t*> blocked_dlls(num_blocked_dlls);
  blacklist::SuccessfullyBlocked(&blocked_dlls[0], &num_blocked_dlls);

  // Send up the hashes of the blocked dlls via UMA.
  for (size_t i = 0; i < blocked_dlls.size(); ++i) {
    std::string dll_name_utf8;
    base::WideToUTF8(blocked_dlls[i], wcslen(blocked_dlls[i]), &dll_name_utf8);
    int uma_hash = DllNameToHash(dll_name_utf8);

    base::UmaHistogramSparse("Blacklist.Blocked", uma_hash);
  }
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

  // Report all successful blacklist interceptions.
  ReportSuccessfulBlocks();

  // Schedule another task to report all successful interceptions later.
  // This time delay should be long enough to catch any dlls that attempt to
  // inject after Chrome has started up.
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&ReportSuccessfulBlocks),
      base::TimeDelta::FromSeconds(kBlacklistReportingDelaySec));

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
    if (blacklist::IsBlacklistInitialized()) {
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
    UMA_HISTOGRAM_COUNTS_100("Blacklist.RetryAttempts.Success", attempt_count);
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
