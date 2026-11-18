// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/dynamic_patch_component.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
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

// Returns the directory within `target_path` for `version` if it exists, or an
// empty path otherwise.
base::FilePath FindVersionDir(const base::FilePath& target_path,
                              const base::Version& version) {
  base::FilePath version_dir = target_path.AppendASCII(version.GetString());
  return base::DirectoryExists(version_dir) ? version_dir : base::FilePath();
}

}  // namespace

// static
std::unique_ptr<DynamicPatchComponent> DynamicPatchComponent::Create(
    const base::CommandLine& command_line,
    const InstallationState& original_state,
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

  return std::make_unique<DynamicPatchComponent>(
      *std::move(sid), std::move(user_data_dir),
      installer_state.GetCurrentVersion(original_state));
}

DynamicPatchComponent::DynamicPatchComponent(base::win::Sid sid,
                                             base::FilePath user_data_dir,
                                             base::Version current_version)
    : sid_(std::move(sid)),
      user_data_dir_(std::move(user_data_dir)),
      current_version_(std::move(current_version)) {}

DynamicPatchComponent::~DynamicPatchComponent() = default;

bool DynamicPatchComponent::ReadManifest(const base::DictValue& manifest) {
  const std::string* name = manifest.FindString("name");
  if (!name || *name != kManifestName) {
    LOG(ERROR) << "Component manifest name mismatch. Expected: "
               << kManifestName << ", actual: " << (name ? *name : "(missing)");
    return false;
  }

  const base::ListValue* compatible_versions =
      manifest.FindList("compatible_versions");
  if (!compatible_versions || compatible_versions->empty()) {
    LOG(ERROR)
        << "Failed to find non-empty compatible_versions in manifest.json";
    return false;
  }

  std::vector<base::Version> versions;
  versions.reserve(compatible_versions->size());
  for (const base::Value& entry : *compatible_versions) {
    const std::string* version_str = entry.GetIfString();
    if (!version_str) {
      LOG(ERROR) << "Non-string entry in compatible_versions in manifest.json";
      return false;
    }
    base::Version version(*version_str);
    if (!version.IsValid() || version.GetString() != *version_str) {
      LOG(ERROR) << "Invalid version in compatible_versions in manifest: "
                 << *version_str;
      return false;
    }
    versions.push_back(std::move(version));
  }
  compatible_versions_ = base::flat_set<base::Version>(std::move(versions));
  return true;
}

base::expected<base::FilePath, InstallStatus>
DynamicPatchComponent::DetermineDestinationRoot(
    const InstallerState& installer_state,
    std::string_view crx_id) {
  CHECK(!compatible_versions_.empty());

  // Install into the currently-running Chrome version's directory if it is
  // compatible and exists on disk. Otherwise, install into the highest
  // compatible version whose directory exists on disk (e.g., an old version
  // that is still running and pending cleanup).
  base::FilePath base_version_dir;
  if (current_version_.IsValid() &&
      compatible_versions_.contains(current_version_)) {
    base_version_dir =
        FindVersionDir(installer_state.target_path(), current_version_);
  }
  if (base_version_dir.empty()) {
    for (const base::Version& version :
         std::views::reverse(compatible_versions_)) {
      if (current_version_.IsValid() && version == current_version_) {
        continue;
      }
      base_version_dir = FindVersionDir(installer_state.target_path(), version);
      if (!base_version_dir.empty()) {
        break;
      }
    }
  }
  if (base_version_dir.empty()) {
    LOG(ERROR) << "No compatible version directory exists in "
               << installer_state.target_path();
    return base::unexpected(INSTALL_COMPONENT_INVALID_INPUT);
  }

  const base::FilePath user_path_component =
      child_module::ComputeUserPathComponent(sid_, user_data_dir_);
  base::FilePath modules_dir =
      child_module::GetModulesDirForUser(base_version_dir, user_path_component);
  if (modules_dir.empty()) {
    LOG(ERROR) << "Failed to compute user path component.";
    return base::unexpected(INSTALL_COMPONENT_FAILED_INTERNAL);
  }
  return modules_dir;
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
