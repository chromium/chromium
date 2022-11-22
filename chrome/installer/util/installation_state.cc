// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installation_state.h"

#include <memory>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/app_commands.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

// Initializes |commands| from the "Commands" subkey of |version_key|. Returns
// false if there is no "Commands" subkey or on error.
bool InitializeCommands(const base::win::RegKey& clients_key,
                        AppCommands* commands) {
  static const DWORD kAccess =
      KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_WOW64_32KEY;
  base::win::RegKey commands_key;

  if (commands_key.Open(clients_key.Handle(), google_update::kRegCommandsKey,
                        kAccess) == ERROR_SUCCESS) {
    return commands->Initialize(commands_key, KEY_WOW64_32KEY);
  }
  return false;
}

}  // namespace

ProductState::ProductState()
    : uninstall_command_(base::CommandLine::NO_PROGRAM),
      eula_accepted_(0),
      usagestats_(0),
      msi_(false),
      has_eula_accepted_(false),
      has_oem_install_(false),
      has_usagestats_(false) {}

ProductState::~ProductState() {}

bool ProductState::Initialize(bool system_install) {
  static const DWORD kAccess = KEY_QUERY_VALUE | KEY_WOW64_32KEY;
  const std::wstring clients_key(install_static::GetClientsKeyPath());
  const std::wstring state_key(install_static::GetClientStateKeyPath());
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey key;

  // Clear the runway.
  Clear();

  // Read from the Clients key.
  if (key.Open(root_key, clients_key.c_str(), kAccess) == ERROR_SUCCESS) {
    std::wstring version_str;
    if (key.ReadValue(google_update::kRegVersionField, &version_str) ==
        ERROR_SUCCESS) {
      version_ =
          std::make_unique<base::Version>(base::WideToASCII(version_str));
      if (!version_->IsValid())
        version_.reset();
    }

    // Attempt to read the other values even if the "pv" version value was
    // absent. Note that ProductState instances containing these values will
    // only be accessible via InstallationState::GetNonVersionedProductState.
    if (key.ReadValue(google_update::kRegOldVersionField, &version_str) ==
        ERROR_SUCCESS) {
      old_version_ =
          std::make_unique<base::Version>(base::WideToASCII(version_str));
      if (!old_version_->IsValid())
        old_version_.reset();
    }

    if (!InitializeCommands(key, &commands_))
      commands_.Clear();
  }

  // Read from the ClientState key.
  if (key.Open(root_key, state_key.c_str(), kAccess) == ERROR_SUCCESS) {
    std::wstring setup_path;
    std::wstring uninstall_arguments;

    // Read in the brand code, it may be absent
    key.ReadValue(google_update::kRegBrandField, &brand_);

    key.ReadValue(kUninstallStringField, &setup_path);
    key.ReadValue(kUninstallArgumentsField, &uninstall_arguments);
    InstallUtil::ComposeCommandLine(setup_path, uninstall_arguments,
                                    &uninstall_command_);

    // "usagestats" may be absent, 0 (false), or 1 (true).  On the chance that
    // different values are permitted in the future, we'll simply hold whatever
    // we find.
    has_usagestats_ = (key.ReadValueDW(google_update::kRegUsageStatsField,
                                       &usagestats_) == ERROR_SUCCESS);
    // "oeminstall" may be present with any value or absent.
    has_oem_install_ = (key.ReadValue(google_update::kRegOemInstallField,
                                      &oem_install_) == ERROR_SUCCESS);
    // "eulaaccepted" may be absent, 0 or 1.
    has_eula_accepted_ = (key.ReadValueDW(google_update::kRegEulaAceptedField,
                                          &eula_accepted_) == ERROR_SUCCESS);
    // "msi" may be absent, 0 or 1
    DWORD dw_value = 0;
    msi_ = (key.ReadValueDW(google_update::kRegMSIField, &dw_value) ==
            ERROR_SUCCESS) &&
           (dw_value != 0);
  }

  // Read from the ClientStateMedium key.  Values here override those in
  // ClientState.
  if (system_install &&
      key.Open(root_key, install_static::GetClientStateMediumKeyPath().c_str(),
               kAccess) == ERROR_SUCCESS) {
    DWORD dword_value = 0;

    if (key.ReadValueDW(google_update::kRegUsageStatsField, &dword_value) ==
        ERROR_SUCCESS) {
      has_usagestats_ = true;
      usagestats_ = dword_value;
    }

    if (key.ReadValueDW(google_update::kRegEulaAceptedField, &dword_value) ==
        ERROR_SUCCESS) {
      has_eula_accepted_ = true;
      eula_accepted_ = dword_value;
    }
  }

  return version_.get() != nullptr;
}

base::FilePath ProductState::GetSetupPath() const {
  return uninstall_command_.GetProgram();
}

const base::Version& ProductState::version() const {
  DCHECK(version_);
  return *version_;
}

ProductState& ProductState::CopyFrom(const ProductState& other) {
  version_.reset(other.version_.get() ? new base::Version(*other.version_)
                                      : nullptr);
  old_version_.reset(other.old_version_.get()
                         ? new base::Version(*other.old_version_)
                         : nullptr);
  brand_ = other.brand_;
  uninstall_command_ = other.uninstall_command_;
  oem_install_ = other.oem_install_;
  commands_.CopyFrom(other.commands_);
  eula_accepted_ = other.eula_accepted_;
  usagestats_ = other.usagestats_;
  msi_ = other.msi_;
  has_eula_accepted_ = other.has_eula_accepted_;
  has_oem_install_ = other.has_oem_install_;
  has_usagestats_ = other.has_usagestats_;

  return *this;
}

void ProductState::Clear() {
  version_.reset();
  old_version_.reset();
  brand_.clear();
  oem_install_.clear();
  uninstall_command_ = base::CommandLine(base::CommandLine::NO_PROGRAM);
  commands_.Clear();
  eula_accepted_ = 0;
  usagestats_ = 0;
  msi_ = false;
  has_eula_accepted_ = false;
  has_oem_install_ = false;
  has_usagestats_ = false;
}

bool ProductState::GetEulaAccepted(DWORD* eula_accepted) const {
  DCHECK(eula_accepted);
  if (!has_eula_accepted_)
    return false;
  *eula_accepted = eula_accepted_;
  return true;
}

bool ProductState::GetOemInstall(std::wstring* oem_install) const {
  DCHECK(oem_install);
  if (!has_oem_install_)
    return false;
  *oem_install = oem_install_;
  return true;
}

bool ProductState::GetUsageStats(DWORD* usagestats) const {
  DCHECK(usagestats);
  if (!has_usagestats_)
    return false;
  *usagestats = usagestats_;
  return true;
}

InstallationState::InstallationState() {}

void InstallationState::Initialize() {
  user_chrome_.Initialize(false);
  system_chrome_.Initialize(true);
}

const ProductState* InstallationState::GetProductState(
    bool system_install) const {
  const ProductState* product_state =
      GetNonVersionedProductState(system_install);
  return product_state->version_.get() ? product_state : nullptr;
}

const ProductState* InstallationState::GetNonVersionedProductState(
    bool system_install) const {
  return system_install ? &system_chrome_ : &user_chrome_;
}

}  // namespace installer
