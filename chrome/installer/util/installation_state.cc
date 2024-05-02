// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installation_state.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "build/build_config.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/app_commands.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
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

    if (key.ReadValue(google_update::kRegChannelField, &channel_) !=
        ERROR_SUCCESS) {
      channel_.clear();
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

    constexpr base::wcstring_view kEnterpriseProductPrefix(
        L"EnterpriseProduct");
    for (base::win::RegistryValueIterator iter(root_key, state_key.c_str(),
                                               KEY_WOW64_32KEY);
         iter.Valid(); ++iter) {
      std::wstring_view value_name(iter.Name());
      if (base::StartsWith(value_name, kEnterpriseProductPrefix,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        product_guid_ = value_name.substr(kEnterpriseProductPrefix.size());
        break;
      }
    }
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

  if (version_.get() && uninstall_command_.GetProgram().empty()) {
    // The product has a "pv" in its Clients key, but is missing the
    // "UninstallString" value expected in its ClientState key. Manufacture the
    // expected uninstall command on the basis of what appears to be present on
    // the machine.

    // FindInstallPath returns the path to a version directory if one exists in
    // any of the standard install locations.
    if (const auto version_dir = FindInstallPath(system_install, *version_);
        !version_dir.empty()) {
      uninstall_command_ = base::CommandLine(
          version_dir.Append(kInstallerDir).Append(kSetupExe));
      uninstall_command_.AppendSwitch(switches::kUninstall);
      InstallUtil::AppendModeAndChannelSwitches(&uninstall_command_);

      // Note that `AppendModeAndChannelSwitches` will use the current process's
      // channel. When called from within a browser process, this will be
      // correct. When called from within the installer, it will not be. To
      // ensure that the browser's notion of the channel is always used, strip
      // off a value added above and explicitly add the value from the Clients
      // key.
      uninstall_command_.RemoveSwitch(switches::kChannel);
      if (!channel_.empty()) {
        uninstall_command_.AppendSwitchNative(switches::kChannel, channel_);
      }

      if (system_install) {
        uninstall_command_.AppendSwitch(switches::kSystemLevel);
      }
      uninstall_command_.AppendSwitch(switches::kVerboseLogging);
    }
  }

  if (version_.get() && system_install && (!msi_ || product_guid_.empty())) {
    // A system-level install may be missing the "msi" and/or
    // "EnterpriseProduct" values in ClientState. To repair from this, search
    // for an ARP entry under a product guid with a display name matching this
    // product.
    if (auto product_guid =
            FindProductGuid(InstallUtil::GetDisplayName(), product_guid_);
        !product_guid.empty()) {
      msi_ = true;
      product_guid_ = std::move(product_guid);
      // Add `--msi` to the uninstall args if it is missing.
      if (!uninstall_command_.HasSwitch(switches::kMsi)) {
        uninstall_command_.AppendSwitch(switches::kMsi);
      }
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
  channel_ = other.channel_;
  brand_ = other.brand_;
  uninstall_command_ = other.uninstall_command_;
  product_guid_ = other.product_guid_;
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
  channel_.clear();
  brand_.clear();
  oem_install_.clear();
  uninstall_command_ = base::CommandLine(base::CommandLine::NO_PROGRAM);
  product_guid_.clear();
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

// static
std::wstring ProductState::FindProductGuid(std::wstring_view display_name,
                                           std::wstring_view hint) {
  constexpr base::wcstring_view kUninstallRootKey(
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
  constexpr size_t kGuidLength = 36;  // Does not include braces.
  constexpr REGSAM kViews[] = {
      0,  // The default view for this bitness.
#if defined(ARCH_CPU_64_BITS)
      KEY_WOW64_32KEY,  // 32-bit view.
#else
      KEY_WOW64_64KEY,  // 64-bit view.
#endif
  };
  // Returns true if the DisplayName value for the uninstall entry for `name` in
  // `view` of HKLM equals `display_name`.
  auto display_name_is = [&kUninstallRootKey, storage = std::wstring()](
                             REGSAM view, std::wstring_view name,
                             std::wstring_view display_name) mutable {
    return base::win::RegKey(HKEY_LOCAL_MACHINE,
                             base::StrCat({kUninstallRootKey, name}).c_str(),
                             KEY_QUERY_VALUE | view)
                   .ReadValue(kUninstallDisplayNameField, &storage) ==
               ERROR_SUCCESS &&
           storage == display_name;
  };

  // If the caller provided a hint, look first for an entry for it.
  if (!hint.empty()) {
    const std::wstring name = base::StrCat({L"{", hint, L"}"});
    for (const REGSAM view : kViews) {
      if (display_name_is(view, name, display_name)) {
        return std::wstring(hint);
      }
    }
  }

  // Otherwise, search through all subkeys named with GUIDs looking for a hit.
  for (const REGSAM view : kViews) {
    for (base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE,
                                             kUninstallRootKey.c_str(), view);
         iter.Valid(); ++iter) {
      const std::wstring_view key_name(iter.Name());
      // Skip this key if it doesn't plausibly look like a product guid.
      if (key_name.size() != kGuidLength + 2 || key_name.front() != L'{' ||
          key_name.back() != L'}') {
        continue;
      }
      if (display_name_is(view, key_name, display_name)) {
        return std::wstring(key_name.substr(1, kGuidLength));
      }
    }
  }

  return {};
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
