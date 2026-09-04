// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_component.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/file_conductor.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crx_file/crx_verifier.h"
#include "components/crx_file/id_util.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/zlib/google/zip.h"

namespace installer {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the PlatformRuntime
// component.
constexpr uint8_t kPlatformRuntimePublicKeySHA256[32] = {
    0x98, 0x34, 0x28, 0xc0, 0x5e, 0x1e, 0x60, 0x76, 0xb8, 0x2f, 0xc4,
    0x09, 0x20, 0x00, 0x81, 0x81, 0x76, 0x2f, 0x59, 0xa6, 0x57, 0x67,
    0x42, 0xd1, 0xfe, 0xdf, 0xd0, 0x28, 0x86, 0x10, 0xef, 0xf0};

constexpr ComponentConfig kSupportedComponents[] = {
    {"Chrome Platform Runtime (Inner)", kPlatformRuntimePublicKeySHA256},
};

InstallStatus InstallComponentInternal(
    const base::FilePath& source_file,
    const InstallerState& installer_state,
    base::span<const ComponentConfig> supported_components,
    crx_file::VerifierFormat verifier_format) {
  // Validate source file: must not be empty and must not reference parent dirs.
  CHECK(!source_file.empty() && !source_file.ReferencesParent());

  // TOCTOU-Safe Staged copy into admin-protected temp directory.
  SelfCleaningTempDir temp_path;
  if (!temp_path.Initialize(installer_state.target_path().DirName(),
                            kInstallTempDir)) {
    PLOG(ERROR) << "Failed to initialize temporary directory";
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  FileConductor file_conductor(temp_path.path());
  absl::Cleanup undo_on_failure = [&file_conductor] {
    VLOG(1) << "Failure occurred, calling FileConductor::Undo()";
    file_conductor.Undo();
  };

  base::FilePath staged_crx_dir;
  if (!base::CreateTemporaryDirInDir(temp_path.path(), L"CrxStaging",
                                     &staged_crx_dir)) {
    PLOG(ERROR) << "Failed to create CRX staging subdirectory in "
                << temp_path.path();
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  base::FilePath staged_file = staged_crx_dir.Append(source_file.BaseName());
  if (!file_conductor.CopyEntry(source_file, staged_file)) {
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  std::string public_key;
  std::string crx_id;
  // Verify CRX signature and retrieve its developer public key and CRX ID.
  if (crx_file::Verify(staged_file, verifier_format,
                       /*required_key_hashes=*/{}, /*required_file_hash=*/{},
                       &public_key, &crx_id,
                       /*compressed_verified_contents=*/nullptr) !=
      crx_file::VerifierResult::OK_FULL) {
    LOG(ERROR) << "CRX verification failed.";
    return installer::INSTALL_COMPONENT_FAILED_SIGNATURE;
  }

  auto it = std::ranges::find_if(
      supported_components, [&](const ComponentConfig& config) {
        return crx_file::id_util::GenerateIdFromHash(
                   config.public_key_sha256) == crx_id;
      });
  if (it == supported_components.end()) {
    LOG(ERROR) << "Unsupported component CRX ID: " << crx_id;
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }
  const ComponentConfig& component_config = *it;

  std::optional<std::vector<uint8_t>> public_key_bytes =
      base::Base64Decode(public_key);
  if (!public_key_bytes.has_value()) {
    LOG(ERROR) << "Failed to decode CRX public key.";
    return installer::INSTALL_COMPONENT_FAILED_SIGNATURE;
  }

  const auto public_key_sha256 = crypto::hash::Sha256(*public_key_bytes);
  if (!std::ranges::equal(public_key_sha256,
                          component_config.public_key_sha256)) {
    LOG(ERROR) << "Component public key hash mismatch for " << crx_id;
    return installer::INSTALL_COMPONENT_FAILED_SIGNATURE;
  }

  // Fresh directory for unpacking the CRX payload to prevent filename
  // collisions with the staged CRX file.
  base::FilePath unpack_dir;
  if (!base::CreateTemporaryDirInDir(temp_path.path(), L"Unpacked",
                                     &unpack_dir)) {
    PLOG(ERROR) << "Failed to create unpack subdirectory in "
                << temp_path.path();
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // Unpack CRX into the fresh unpack_dir.
  if (!zip::Unzip(staged_file, unpack_dir)) {
    LOG(ERROR) << "Failed to unpack CRX.";
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  // Read manifest.json to get version and validate name.
  base::FilePath manifest_path =
      unpack_dir.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest_path)) {
    LOG(ERROR) << "manifest.json missing in unpacked component.";
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  JSONFileValueDeserializer deserializer(manifest_path);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root || !root->is_dict()) {
    LOG(ERROR) << "Failed to parse manifest.json: " << error;
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  const std::string* manifest_name = root->GetDict().FindString("name");
  if (!manifest_name || *manifest_name != component_config.manifest_name) {
    LOG(ERROR) << "Component manifest name mismatch. Expected: "
               << component_config.manifest_name << ", actual: "
               << (manifest_name ? *manifest_name : "(missing)");
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  const std::string* manifest_version = root->GetDict().FindString("version");
  if (!manifest_version) {
    LOG(ERROR) << "Failed to find version in manifest.json";
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }
  const base::Version component_version(*manifest_version);
  if (!component_version.IsValid()) {
    LOG(ERROR) << "Invalid version in manifest: " << *manifest_version;
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  // Verify no newer version is already installed.
  base::FilePath component_root =
      installer_state.target_path().Append(base::FilePath::FromASCII(crx_id));
  std::optional<base::Version> highest_version =
      FindHighestComponentVersion(component_root);
  if (highest_version && *highest_version >= component_version) {
    LOG(INFO) << "Component is already installed at a same or higher version: "
              << highest_version->GetString()
              << " >= " << component_version.GetString();
    return installer::INSTALL_COMPONENT_ALREADY_EXISTS;
  }

  // Derive fixed destination in
  // C:\Program Files\...\<component_dir>\<version>
  base::FilePath target_dir =
      component_root.AppendASCII(component_version.GetString());

  // Remove any scheduled MOVEFILE_DELAY_UNTIL_REBOOT entries in the target of
  // this installation.
  if (installer_state.system_install()) {
    if (!RemoveFromMovesPendingReboot(target_dir)) {
      PLOG(ERROR) << "Error accessing pending moves value for " << target_dir;
    }
  }

  // Ensure intermediate directories exist (e.g. component_root).
  if (!base::CreateDirectory(component_root)) {
    PLOG(ERROR) << "Failed to create component root directory: "
                << component_root;
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // Delete the target directory if it exists to clean up any previous
  // installation attempts. Using FileConductor allows rollback on failure.
  if (!file_conductor.DeleteEntry(target_dir)) {
    PLOG(WARNING) << "Failed to delete existing target directory: "
                  << target_dir;
  }

  // Atomic move to final versioned destination. Move the whole unpack_dir.
  if (!file_conductor.MoveEntry(unpack_dir, target_dir,
                                /*lenient_deletion=*/true)) {
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // Success, cancel the automatic undo.
  std::move(undo_on_failure).Cancel();
  DeleteInvalidComponentDirectories(component_root, component_version);
  return installer::INSTALL_COMPONENT_SUCCESS;
}

}  // namespace

InstallStatus InstallComponent(const base::FilePath& source_file,
                               const InstallerState& installer_state) {
  return InstallComponentInternal(
      source_file, installer_state, kSupportedComponents,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF);
}

InstallStatus InstallComponentForTesting(
    const base::FilePath& source_file,
    const InstallerState& installer_state,
    base::span<const ComponentConfig> supported_components,
    crx_file::VerifierFormat verifier_format) {
  return InstallComponentInternal(source_file, installer_state,
                                  supported_components, verifier_format);
}

std::optional<base::Version> GetComponentVersion(
    const base::FilePath& version_dir) {
  base::Version version(version_dir.BaseName().MaybeAsASCII());
  if (!version.IsValid()) {
    return std::nullopt;
  }
  if (!base::PathExists(
          version_dir.Append(FILE_PATH_LITERAL("manifest.json")))) {
    return std::nullopt;
  }
  return version;
}

std::optional<base::Version> FindHighestComponentVersion(
    const base::FilePath& component_root) {
  std::optional<base::Version> highest_version;
  base::FileEnumerator(component_root, /*recursive=*/false,
                       base::FileEnumerator::DIRECTORIES)
      .ForEach([&highest_version](const base::FilePath& existing_dir) {
        std::optional<base::Version> version =
            GetComponentVersion(existing_dir);
        if (version && (!highest_version || *version > *highest_version)) {
          highest_version = std::move(version);
        }
      });
  return highest_version;
}

void DeleteInvalidComponentDirectories(const base::FilePath& component_root,
                                       const base::Version& keep_version) {
  base::FileEnumerator(component_root, /*recursive=*/false,
                       base::FileEnumerator::DIRECTORIES)
      .ForEach([&keep_version](const base::FilePath& existing_dir) {
        std::optional<base::Version> existing_version =
            GetComponentVersion(existing_dir);
        if (!existing_version.has_value() || *existing_version < keep_version) {
          if (base::DeletePathRecursively(existing_dir)) {
            VLOG(1) << "Deleted old or invalid component directory: "
                    << existing_dir;
          } else {
            PLOG(WARNING)
                << "Failed to delete old or invalid component directory "
                << existing_dir;
            ScheduleDirectoryForDeletion(existing_dir);
          }
        }
      });
}

}  // namespace installer
