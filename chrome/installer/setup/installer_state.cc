// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_state.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

namespace {

// Returns the boolean value of the distribution preference in |prefs| named
// |pref_name|, or |default_value| if not set.
bool GetMasterPreference(const MasterPreferences& prefs,
                         const char* pref_name,
                         bool default_value) {
  bool value;
  return prefs.GetBool(pref_name, &value) ? value : default_value;
}

}  // namespace

InstallerState::InstallerState()
    : operation_(UNINITIALIZED),
      level_(UNKNOWN_LEVEL),
      root_key_(NULL),
      msi_(false),
      verbose_logging_(false),
      is_migrating_to_single_(false) {}

InstallerState::InstallerState(Level level)
    : operation_(UNINITIALIZED),
      level_(UNKNOWN_LEVEL),
      root_key_(NULL),
      msi_(false),
      verbose_logging_(false),
      is_migrating_to_single_(false) {
  // Use set_level() so that root_key_ is updated properly.
  set_level(level);
}

InstallerState::~InstallerState() {
}

void InstallerState::Initialize(const base::CommandLine& command_line,
                                const MasterPreferences& prefs,
                                const InstallationState& machine_state) {
  Clear();

  set_level(GetMasterPreference(prefs, master_preferences::kSystemLevel, false)
                ? SYSTEM_LEVEL
                : USER_LEVEL);

  verbose_logging_ =
      GetMasterPreference(prefs, master_preferences::kVerboseLogging, false);

  msi_ = GetMasterPreference(prefs, master_preferences::kMsi, false);
  if (!msi_) {
    const ProductState* product_state =
        machine_state.GetProductState(system_install());
    if (product_state != NULL)
      msi_ = product_state->is_msi();
  }

  const bool is_uninstall = command_line.HasSwitch(switches::kUninstall);

  // TODO(grt): Infer target_path_ from an existing install in support of
  // varying install locations; see https://crbug.com/380177.
  target_path_ = GetChromeInstallPath(system_install());
  state_key_ = install_static::GetClientStateKeyPath();

  VLOG(1) << (is_uninstall ? "Uninstall Chrome" : "Install Chrome");

  if (is_uninstall) {
    operation_ = UNINSTALL;
  } else {
    operation_ = SINGLE_INSTALL_OR_UPDATE;
    // Is this a migration from multi-install to single-install?
    const ProductState* state = machine_state.GetProductState(system_install());
    is_migrating_to_single_ = state && state->is_multi_install();
  }

  // Parse --critical-update-version=W.X.Y.Z
  std::string critical_version_value(
      command_line.GetSwitchValueASCII(switches::kCriticalUpdateVersion));
  critical_update_version_ = base::Version(critical_version_value);
}

void InstallerState::set_level(Level level) {
  level_ = level;
  switch (level) {
    case UNKNOWN_LEVEL:
      root_key_ = nullptr;
      return;
    case USER_LEVEL:
      root_key_ = HKEY_CURRENT_USER;
      return;
    case SYSTEM_LEVEL:
      root_key_ = HKEY_LOCAL_MACHINE;
      return;
  }
  NOTREACHED() << level;
}

bool InstallerState::system_install() const {
  DCHECK(level_ == USER_LEVEL || level_ == SYSTEM_LEVEL);
  return level_ == SYSTEM_LEVEL;
}

base::Version* InstallerState::GetCurrentVersion(
    const InstallationState& machine_state) const {
  std::unique_ptr<base::Version> current_version;
  const ProductState* product_state =
      machine_state.GetProductState(level_ == SYSTEM_LEVEL);

  if (product_state != NULL) {
    const base::Version* version = NULL;

    // Be aware that there might be a pending "new_chrome.exe" already in the
    // installation path.  If so, we use old_version, which holds the version of
    // "chrome.exe" itself.
    if (base::PathExists(target_path().Append(kChromeNewExe)))
      version = product_state->old_version();

    if (version == NULL)
      version = &product_state->version();

    current_version.reset(new base::Version(*version));
  }

  return current_version.release();
}

base::Version InstallerState::DetermineCriticalVersion(
    const base::Version* current_version,
    const base::Version& new_version) const {
  DCHECK(current_version == NULL || current_version->IsValid());
  DCHECK(new_version.IsValid());
  if (critical_update_version_.IsValid() &&
      (current_version == NULL ||
       (current_version->CompareTo(critical_update_version_) < 0)) &&
      new_version.CompareTo(critical_update_version_) >= 0) {
    return critical_update_version_;
  }
  return base::Version();
}

base::FilePath InstallerState::GetInstallerDirectory(
    const base::Version& version) const {
  return target_path().AppendASCII(version.GetString()).Append(kInstallerDir);
}

void InstallerState::Clear() {
  operation_ = UNINITIALIZED;
  target_path_.clear();
  state_key_.clear();
  critical_update_version_ = base::Version();
  level_ = UNKNOWN_LEVEL;
  root_key_ = NULL;
  msi_ = false;
  verbose_logging_ = false;
  is_migrating_to_single_ = false;
}

void InstallerState::SetStage(InstallerStage stage) const {
  GoogleUpdateSettings::SetProgress(system_install(), state_key_,
                                    progress_calculator_.Calculate(stage));
}

void InstallerState::UpdateChannels() const {
  DCHECK_NE(UNINSTALL, operation_);
  // Update the "ap" value for the product being installed/updated.  Use the
  // current value in the registry since the InstallationState instance used by
  // the bulk of the installer does not track changes made by UpdateStage.
  // Create the app's ClientState key if it doesn't exist.
  ChannelInfo channel_info;
  base::win::RegKey state_key;
  LONG result =
      state_key.Create(root_key_,
                       state_key_.c_str(),
                       KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_SUCCESS) {
    channel_info.Initialize(state_key);

    // Multi-install has been deprecated. All installs and updates are single.
    bool modified = channel_info.SetMultiInstall(false);

    // Remove all multi-install products from the channel name.
    modified |= channel_info.SetChrome(false);
    modified |= channel_info.SetChromeFrame(false);
    modified |= channel_info.SetAppLauncher(false);

    VLOG(1) << "ap: " << channel_info.value();

    // Write the results if needed.
    if (modified)
      channel_info.Write(&state_key);
  } else {
    LOG(ERROR) << "Failed opening key " << state_key_
               << " to update app channels; result: " << result;
  }
}

void InstallerState::WriteInstallerResult(
    InstallStatus status,
    int string_resource_id,
    const base::string16* const launch_cmd) const {
  // Use a no-rollback list since this is a best-effort deal.
  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->set_log_message("Write Installer Result");
  install_list->set_best_effort(true);
  install_list->set_rollback_enabled(false);
  const bool system_install = this->system_install();
  // Write the value for the product upon which we're operating.
  InstallUtil::AddInstallerResultItems(
      system_install, install_static::GetClientStateKeyPath(), status,
      string_resource_id, launch_cmd, install_list.get());
  if (is_migrating_to_single() && InstallUtil::GetInstallReturnCode(status)) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Write to the binaries on error if this is a migration back to
    // single-install for Google Chrome builds. Skip this for Chromium builds
    // because they lump the "ClientState" and "Clients" keys into a single
    // key. As a consequence, writing this value causes Software\Chromium to be
    // re-created after it was deleted during the migration to single-install.
    // Google Chrome builds don't suffer this since the two keys are distinct
    // and have different lifetimes. The result is only written on failure since
    // for success, the binaries have been uninstalled and therefore the result
    // will not be read by Google Update.
    InstallUtil::AddInstallerResultItems(
        system_install, install_static::GetClientStateKeyPathForBinaries(),
        status, string_resource_id, launch_cmd, install_list.get());
#endif
  }
  install_list->Do();
}

bool InstallerState::RequiresActiveSetup() const {
  return system_install();
}

}  // namespace installer
