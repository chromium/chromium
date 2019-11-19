// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "components/metrics/client_info.h"

namespace installer {
class ChannelInfo;
class InstallationState;
}

// This class provides accessors to the Google Update group policies and
// 'ClientState' information. The group policies are set using specific
// administrative templates. The 'ClientState' information is recorded when the
// user downloads the Chrome installer. It is google_update.exe responsibility
// to write the initial values.
class GoogleUpdateSettings {
 public:
  // Update policy constants defined by Google Update; do not change these.
  enum UpdatePolicy {
    UPDATES_DISABLED    = 0,
    AUTOMATIC_UPDATES   = 1,
    MANUAL_UPDATES_ONLY = 2,
    AUTO_UPDATES_ONLY   = 3,
    UPDATE_POLICIES_COUNT
  };

  static const wchar_t kPoliciesKey[];
  static const wchar_t kUpdatePolicyValue[];
  static const wchar_t kUpdateOverrideValuePrefix[];
  static const wchar_t kCheckPeriodOverrideMinutes[];
  static const wchar_t kDownloadPreferencePolicyValue[];
  static const int kCheckPeriodOverrideMinutesDefault;
  static const int kCheckPeriodOverrideMinutesMax;
  static const GoogleUpdateSettings::UpdatePolicy kDefaultUpdatePolicy;

  // Defines product data that is tracked/used by Google Update.
  struct ProductData {
    // The currently installed version.
    std::string version;
    // The time that Google Update last updated this product.  (This means
    // either running an updater successfully, or doing an update check that
    // results in no update available.)
    base::Time last_success;
    // The result reported by the most recent run of an installer/updater.
    int last_result;
    // The error code, if any, reported by the most recent run of an
    // installer or updater.  This is typically platform independent.
    int last_error_code;
    // The extra error code, if any, reported by the most recent run of
    // an installer or updater.  This is typically an error code specific
    // to the platform -- i.e. on Windows, it will be a Win32 HRESULT.
    int last_extra_code;
  };

  // Returns true if this install is system-wide, false if it is per-user.
  static bool IsSystemInstall();

  // Returns the SequencedTaskRunner to be used to sequence calls to
  // Get/SetCollectStatsConsent(). Tasks posted through this will run with
  // USER_VISIBLE priority and will block shutdown.
  // Note: There is not enforcement to ensure that all such calls go through
  // this SequencedTaskRunner but callers that don't are responsible to ensure
  // nothing else is racing with them (e.g. those calls can be called
  // synchronously on first run, startup, etc.).
  static base::SequencedTaskRunner* CollectStatsConsentTaskRunner();

  // Returns whether the user has given consent to collect UMA data and send
  // crash dumps to Google. This information is collected by the web server
  // used to download the chrome installer.
  static bool GetCollectStatsConsent();

  // Sets the user consent to send UMA and crash dumps to Google. Returns
  // false if the setting could not be recorded.
  static bool SetCollectStatsConsent(bool consented);

#if defined(OS_WIN)
  // Returns whether the user has given consent to collect UMA data and send
  // crash dumps to Google for the deprecated Chrome binaries.
  static google_update::Tristate GetCollectStatsConsentForBinaries();

  // Returns the default (original) state of the "send usage stats" checkbox
  // shown to the user when they downloaded Chrome. The value is returned via
  // the out parameter |stats_consent_default|. This function returns true if
  // the default state is known and false otherwise. If false the out param
  // will not be set.
  static bool GetCollectStatsConsentDefault(bool* stats_consent_default)
      WARN_UNUSED_RESULT;
#endif

  // Returns the metrics client info backed up in the registry. NULL
  // if-and-only-if the client_id couldn't be retrieved (failure to retrieve
  // other fields only makes them keep their default value). A non-null return
  // will NEVER contain an empty client_id field.
  static std::unique_ptr<metrics::ClientInfo> LoadMetricsClientInfo();

  // Stores a backup of the metrics client info in the registry. Storing a
  // |client_info| with an empty client id will effectively void the backup.
  static void StoreMetricsClientInfo(const metrics::ClientInfo& client_info);

  // Sets the machine-wide EULA consented flag required on OEM installs.
  // Returns false if the setting could not be recorded.
  static bool SetEulaConsent(const installer::InstallationState& machine_state,
                             bool consented);

  // Returns the last time chrome was run in days. It uses a recorded value
  // set by SetLastRunTime(). Returns -1 if the value was not found or if
  // the value is corrupted.
  static int GetLastRunTime();

  // Stores the time that this function was last called using an encoded
  // form of the system local time. Retrieve the time using GetLastRunTime().
  // Returns false if the value could not be stored.
  static bool SetLastRunTime();

  // Removes the storage used by SetLastRunTime() and SetLastRunTime(). Returns
  // false if the operation failed. Returns true if the storage was freed or
  // if it never existed in the first place.
  static bool RemoveLastRunTime();

  // Returns in |browser| the browser used to download chrome as recorded
  // Google Update. Returns false if the information is not available.
  static bool GetBrowser(base::string16* browser);

  // Returns in |language| the language selected by the user when downloading
  // chrome. This information is collected by the web server used to download
  // the chrome installer. Returns false if the information is not available.
  static bool GetLanguage(base::string16* language);

  // Returns in |brand| the RLZ brand code or distribution tag that has been
  // assigned to a partner. Returns false if the information is not available.
  //
  // NOTE: This function is Windows only.  If the code you are writing is not
  // specifically for Windows, prefer calling google_brand::GetBrand().
  static bool GetBrand(base::string16* brand);

  // Returns in |brand| the RLZ reactivation brand code or distribution tag
  // that has been assigned to a partner for reactivating a dormant chrome
  // install. Returns false if the information is not available.
  //
  // NOTE: This function is Windows only.  If the code you are writing is not
  // specifically for Windows, prefer calling
  // google_brand::GetReactivationBrand().
  static bool GetReactivationBrand(base::string16* brand);

  // Returns in 'client' the RLZ referral available for some distribution
  // partners. This value does not exist for most chrome or chromium installs.
  static bool GetReferral(base::string16* referral);

  // Overwrites the current value of the referral with an empty string. Returns
  // true if this operation succeeded.
  static bool ClearReferral();

  // Updates Chrome's "did run" state, returning true if the update succeeds.
  static bool UpdateDidRunState(bool did_run);

  // This method changes the Google Update "ap" value to move the installation
  // on to or off of one of the recovery channels.
  // - If incremental installer fails we append a magic string ("-full"), if
  // it is not present already, so that Google Update server next time will send
  // full installer to update Chrome on the local machine
  // - If we are currently running full installer, we remove this magic
  // string (if it is present) regardless of whether installer failed or not.
  // There is no fall-back for full installer :)
  // - Unconditionally remove "-multifail" since we haven't crashed.
  // |state_key| should be obtained via InstallerState::state_key().
  // - Unconditionally clear a legacy "-stage:" modifier.
  static void UpdateInstallStatus(bool system_install,
                                  installer::ArchiveType archive_type,
                                  int install_return_code,
                                  const base::string16& product_guid);

  // Sets the InstallerProgress value in the registry so that Google Update can
  // provide informative user feedback. |path| is the full path to the app's
  // ClientState key. |progress| should be a number between 0 and 100,
  // inclusive.
  static void SetProgress(bool system_install,
                          const base::string16& path,
                          int progress);

  // This method updates the value for Google Update "ap" key for Chrome
  // based on whether we are doing incremental install (or not) and whether
  // the install succeeded.
  // - If install worked, remove the magic string (if present).
  // - If incremental installer failed, append a magic string (if
  //   not present already).
  // - If full installer failed, still remove this magic
  //   string (if it is present already).
  // Additionally, any legacy "-multifail" or "-stage:*" values are
  // unconditionally removed.
  //
  // archive_type: tells whether this is incremental install or not.
  // install_return_code: if 0, means installation was successful.
  // value: current value of Google Update "ap" key.
  // Returns true if |value| is modified.
  static bool UpdateGoogleUpdateApKey(installer::ArchiveType archive_type,
                                      int install_return_code,
                                      installer::ChannelInfo* value);

  // Returns the effective update policy for |app_guid| as dictated by
  // Group Policy settings.  |is_overridden|, if non-NULL, is populated with
  // true if an app-specific policy override is in force, or false otherwise.
  static UpdatePolicy GetAppUpdatePolicy(base::StringPiece16 app_guid,
                                         bool* is_overridden);

  // Returns true if Chrome should be updated automatically by Google Update
  // based on current autoupdate settings. This is distinct from
  // GetAppUpdatePolicy (which checks only the policy for a given app), as it
  // checks for general Google Update configuration as well as multi-install
  // Chrome. Note that for Chromium builds, this returns false since Chromium is
  // assumed not to autoupdate.
  static bool AreAutoupdatesEnabled();

  // Attempts to reenable auto-updates for Chrome by removing any group policy
  // settings that would block updates from occurring. This is a superset of the
  // things checked by GetAppUpdatePolicy() as GetAppUpdatePolicy() does not
  // check Omaha's AutoUpdateCheckPeriodMinutes setting which will be reset by
  // this method. Will need to be called from an elevated process since those
  // settings live in HKLM. Returns true if there is a reasonable belief that
  // updates are not disabled by policy when this method returns, false
  // otherwise. Note that for Chromium builds, this returns true since Chromium
  // is assumed not to autoupdate.
  static bool ReenableAutoupdates();

  // Returns a string if the corresponding Google Update group policy is set.
  // Returns an empty string if no policy or an invalid policy is set.
  // A valid policy for DownloadPreference is a string that matches the
  // following regex:  `[a-zA-z]{0-32}`. The actual values for this policy
  // are specific to Google Update and documented as part of the Google Update
  // protocol.
  static base::string16 GetDownloadPreference();

  // Returns Google Update's uninstall command line, or an empty string if none
  // is found.
  static base::string16 GetUninstallCommandLine(bool system_install);

  // Returns the version of Google Update that is installed.
  static base::Version GetGoogleUpdateVersion(bool system_install);

  // Returns the time at which Google Update last started an automatic update
  // check, or the null time if this information isn't available.
  static base::Time GetGoogleUpdateLastStartedAU(bool system_install);

  // Returns the time at which Google Update last successfully contacted Google
  // servers and got a valid check response, or the null time if this
  // information isn't available.
  static base::Time GetGoogleUpdateLastChecked(bool system_install);

  // Returns detailed update data for a product being managed by Google Update.
  // Returns true if the |version| and |last_updated| fields in |data|
  // are modified.  The other items are considered optional.
  static bool GetUpdateDetailForApp(bool system_install,
                                    const wchar_t* app_guid,
                                    ProductData* data);

  // Returns product data for Google Update.  (Equivalent to calling
  // GetUpdateDetailForAppGuid with the app guid for Google Update itself.)
  static bool GetUpdateDetailForGoogleUpdate(ProductData* data);

  // Returns product data for the current product. (Equivalent to calling
  // GetUpdateDetailForApp with the current install mode's app guid.)
  static bool GetUpdateDetail(ProductData* data);

  // Sets |experiment_labels| as the Google Update experiment_labels value in
  // the ClientState key for this Chrome product, if appropriate. If
  // |experiment_labels| is empty, this will delete the value instead. This will
  // return true if the label was successfully set (or deleted), false otherwise
  // (even if the label does not need to be set for this particular brand).
  static bool SetExperimentLabels(const base::string16& experiment_labels);

  // Reads the Google Update experiment_labels value in the ClientState key for
  // this Chrome product and writes it into |experiment_labels|. If the key or
  // value does not exist, |experiment_labels| will be set to the empty string.
  // If this brand does not set the experiment_labels value, this will do
  // nothing to |experiment_labels|. This will return true if the label did not
  // exist, or was successfully read.
  static bool ReadExperimentLabels(base::string16* experiment_labels);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
