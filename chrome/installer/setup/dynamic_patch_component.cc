// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/dynamic_patch_component.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/access_control_list.h"
#include "base/win/access_token.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/installer/setup/configure_app_container_sandbox.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

// The name of the component in its manifest.
constexpr char kManifestName[] = "Chrome Dynamic Patch (Inner)";

}  // namespace

// static
std::unique_ptr<DynamicPatchComponent> DynamicPatchComponent::Create(
    const base::CommandLine& command_line,
    const InstallerState& installer_state) {
  base::FilePath user_data_dir =
      command_line.GetSwitchValuePath(switches::kUdd);
  if (user_data_dir.empty()) {
    LOG(ERROR) << "Missing or empty --" << switches::kUdd
               << " switch for dynamic patch.";
    return nullptr;
  }
  // The browser's DIR_USER_DATA is always absolute. Reject relative paths,
  // which would otherwise be resolved against this process's working directory
  // and produce a different component than the browser. Beyond this, the value
  // is treated as an opaque string: the browser is responsible for
  // canonicalizing it (via `child_module::CanonicalizeUserDataDir()`). This
  // process must not do so itself, since it may be running as a different user
  // (e.g., SYSTEM) for which the path may resolve differently.
  if (!user_data_dir.IsAbsolute()) {
    LOG(ERROR) << "Relative --" << switches::kUdd
               << " switch for dynamic patch: " << user_data_dir;
    return nullptr;
  }
  // A canonical path never contains ".." components. This is not a full
  // canonicalization check (which would require accessing the path); it only
  // catches an obvious error on the part of the caller.
  if (user_data_dir.ReferencesParent()) {
    LOG(ERROR) << "Non-canonical --" << switches::kUdd
               << " switch for dynamic patch: " << user_data_dir;
    return nullptr;
  }

  std::optional<base::win::Sid> sid;
  if (installer_state.system_install()) {
    std::wstring user_sid_switch =
        command_line.GetSwitchValueNative(switches::kUserSid);
    sid = base::win::Sid::FromSddlString(user_sid_switch);
    if (!sid.has_value()) {
      LOG(ERROR) << "Invalid or missing --" << switches::kUserSid
                 << " switch for dynamic patch: " << user_sid_switch;
      return nullptr;
    }
  } else if (std::optional<base::win::AccessToken> token =
                 base::win::AccessToken::FromCurrentProcess();
             token.has_value()) {
    sid = token->User();
  } else {
    PLOG(ERROR) << "Failed to determine process AccessToken";
    return nullptr;
  }

  if (!IsUserSid(*sid)) {
    LOG(ERROR) << "Caller SID does not identify a user account: "
               << sid->ToSddlString().value_or(std::wstring());
    return nullptr;
  }

  return std::make_unique<DynamicPatchComponent>(*std::move(sid),
                                                 std::move(user_data_dir));
}

DynamicPatchComponent::DynamicPatchComponent(base::win::Sid sid,
                                             base::FilePath user_data_dir)
    : sid_(std::move(sid)), user_data_dir_(std::move(user_data_dir)) {}

DynamicPatchComponent::~DynamicPatchComponent() = default;

bool DynamicPatchComponent::ReadManifest(const base::DictValue& manifest) {
  const std::string* name = manifest.FindString("name");
  if (!name || *name != kManifestName) {
    LOG(ERROR) << "Component manifest name mismatch. Expected: "
               << kManifestName << ", actual: " << (name ? *name : "(missing)");
    return false;
  }

  const std::string* base_version_str = manifest.FindString("base_version");
  if (!base_version_str) {
    LOG(ERROR) << "Failed to find base_version in manifest.json";
    return false;
  }
  base::Version base_version(*base_version_str);
  if (!base_version.IsValid() ||
      base_version.GetString() != *base_version_str) {
    LOG(ERROR) << "Invalid base_version in manifest: " << *base_version_str;
    return false;
  }
  base_version_ = std::move(base_version);
  return true;
}

base::expected<base::FilePath, InstallStatus>
DynamicPatchComponent::DetermineDestinationRoot(
    const InstallerState& installer_state,
    std::string_view crx_id) {
  CHECK(base_version_.IsValid());

  base::FilePath base_version_dir =
      installer_state.target_path().AppendASCII(base_version_.GetString());
  // Only the existence of the base version's directory is checked; the patch
  // may target a version other than the current one (e.g., an old version
  // that is still running and pending cleanup). That is acceptable: patches
  // live within the base version's directory and are removed along with it.
  if (!base::DirectoryExists(base_version_dir)) {
    LOG(ERROR) << "Base version directory does not exist: " << base_version_dir;
    return base::unexpected(INSTALL_COMPONENT_INVALID_INPUT);
  }

  const base::FilePath user_path_component =
      child_module::ComputeUserPathComponent(sid_, user_data_dir_);
  if (user_path_component.empty()) {
    LOG(ERROR) << "Failed to compute user path component.";
    return base::unexpected(INSTALL_COMPONENT_FAILED_INTERNAL);
  }

  return base_version_dir.Append(child_module::kModulesDirName)
      .Append(user_path_component);
}

bool DynamicPatchComponent::InitializeDestinationRoot(
    const InstallerState& installer_state,
    const base::FilePath& destination_root) {
  if (installer_state.system_install()) {
    // Processes in Chrome's AppContainer sandboxes (e.g., renderers when the
    // RendererAppContainer feature is enabled) are granted access only if one
    // of their capabilities is also granted access. Grant the same
    // capabilities as are granted on the rest of the installation (see
    // ConfigureAppContainerSandbox()). This does not grant access to other
    // users, since access must also be granted to the user.
    std::optional<std::vector<base::win::Sid>> capability_sids =
        GetInstallFilesCapabilitySids();
    if (!capability_sids) {
      PLOG(ERROR) << "Failed to create AppContainer capability SIDs";
      return false;
    }

    std::vector<base::win::ExplicitAccessEntry> entries;
    entries.emplace_back(base::win::WellKnownSid::kLocalSystem,
                         base::win::SecurityAccessMode::kGrant, GENERIC_ALL,
                         CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
    entries.emplace_back(base::win::WellKnownSid::kBuiltinAdministrators,
                         base::win::SecurityAccessMode::kGrant, GENERIC_ALL,
                         CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
    entries.emplace_back(sid_, base::win::SecurityAccessMode::kGrant,
                         FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
                         CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
    for (const base::win::Sid& capability_sid : *capability_sids) {
      entries.emplace_back(capability_sid,
                           base::win::SecurityAccessMode::kGrant,
                           FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
                           CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
    }

    base::win::SecurityDescriptor sd;
    if (!sd.SetDaclEntries(entries)) {
      PLOG(ERROR) << "Failed to create DACL for " << destination_root;
      return false;
    }
    sd.set_dacl_protected(true);
    if (!sd.WriteToFile(destination_root, DACL_SECURITY_INFORMATION)) {
      PLOG(ERROR) << "Failed to set security descriptor on "
                  << destination_root;
      return false;
    }
  }
  return true;
}

bool IsUserSid(const base::win::Sid& sid) {
  PSID psid = sid.GetPSID();
  if (!::IsValidSid(psid)) {
    return false;
  }

  const SID_IDENTIFIER_AUTHORITY* authority = ::GetSidIdentifierAuthority(psid);
  const PUCHAR sub_authority_count = ::GetSidSubAuthorityCount(psid);
  if (!authority || !sub_authority_count || *sub_authority_count != 5) {
    return false;
  }

  // Standard Windows domain or local user account:
  // S-1-5-21-<sub1>-<sub2>-<sub3>-<rid>
  static constexpr SID_IDENTIFIER_AUTHORITY kNtAuthority = {{0, 0, 0, 0, 0, 5}};
  if (base::span(authority->Value) == base::span(kNtAuthority.Value) &&
      *::GetSidSubAuthority(psid, 0) == SECURITY_NT_NON_UNIQUE) {
    const DWORD rid = *::GetSidSubAuthority(psid, 4);
    // User accounts have RID >= 1000, or RID 500 for the built-in
    // Administrator. Built-in domain groups occupy RIDs 512-527.
    return rid >= 1000 || rid == 500;
  }

  // Azure AD / Entra ID user account: S-1-12-1-<sub1>-<sub2>-<sub3>-<sub4>
  static constexpr SID_IDENTIFIER_AUTHORITY kAzureAdAuthority = {
      {0, 0, 0, 0, 0, 12}};
  if (base::span(authority->Value) == base::span(kAzureAdAuthority.Value) &&
      *::GetSidSubAuthority(psid, 0) == 1) {
    return true;
  }

  return false;
}

}  // namespace installer
