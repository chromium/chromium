// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_component.h"

#include <windows.h>

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/function_ref.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/installer/setup/dynamic_patch_component.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/platform_runtime_component.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/file_conductor.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crx_file/crx_verifier.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/zlib/google/zip.h"

namespace installer {

namespace {

// The component's manifest. It is copied into a version directory last, so
// its presence there marks installation of that version as complete.
constexpr base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");

// The browser watches for the same file to know when a dynamic patch is ready.
static_assert(base::FilePath::StringViewType(kManifestFilename) ==
              base::FilePath::StringViewType(child_module::kManifestFilename));

// Pinned SubjectPublicKeyInfo SHA256 of the production dynamic patch component
// key (CRX ID: "binbghhnflgglfabhjocbobkiignbgfh").
constexpr std::array<uint8_t, crypto::hash::kSha256Size>
    kDynamicPatchPublicKeySHA256 = {
        0x18, 0xd1, 0x67, 0x7d, 0x5b, 0x66, 0xb5, 0x01, 0x79, 0xe2, 0x1e,
        0x1a, 0x88, 0x6d, 0x16, 0x57, 0x04, 0x67, 0x63, 0xf0, 0xa2, 0xd1,
        0x0e, 0x39, 0xbf, 0x6d, 0xeb, 0x45, 0x33, 0xe8, 0xda, 0x82};

// Pinned SubjectPublicKeyInfo SHA256 of the production PlatformRuntime
// component key (CRX ID: "jidecimafobogahglicpmeajcaaaibib").
constexpr std::array<uint8_t, crypto::hash::kSha256Size>
    kPlatformRuntimePublicKeySHA256 = {
        0x98, 0x34, 0x28, 0xc0, 0x5e, 0x1e, 0x60, 0x76, 0xb8, 0x2f, 0xc4,
        0x09, 0x20, 0x00, 0x81, 0x81, 0x76, 0x2f, 0x59, 0xa6, 0x57, 0x67,
        0x42, 0xd1, 0xfe, 0xdf, 0xd0, 0x28, 0x86, 0x10, 0xef, 0xf0};

// Factory function type that creates a ComponentInterface from a CommandLine.
using ComponentFactoryFunction =
    std::unique_ptr<ComponentInterface> (*)(const base::CommandLine&,
                                            const InstallationState&,
                                            const InstallerState&);

// Helper function template for calling the static Create member function of an
// implementation's class.
template <typename ComponentImpl>
std::unique_ptr<ComponentInterface> CreateComponent(
    const base::CommandLine& command_line,
    const InstallationState& original_state,
    const InstallerState& installer_state) {
  return ComponentImpl::Create(command_line, original_state, installer_state);
}

constexpr auto kSupportedComponents =
    base::MakeFixedFlatMap<std::array<uint8_t, crypto::hash::kSha256Size>,
                           ComponentFactoryFunction>({
        {kPlatformRuntimePublicKeySHA256,
         &CreateComponent<PlatformRuntimeComponent>},
        {kDynamicPatchPublicKeySHA256, &CreateComponent<DynamicPatchComponent>},
    });

// The name under which the CRX is staged. Using a fixed name keeps the name of
// the caller's file out of the staging path.
constexpr base::FilePath::CharType kStagedCrxFilename[] =
    FILE_PATH_LITERAL("component.crx3");

// Opens `source_file` for reading. Returns an invalid file if it cannot be
// opened or if it is not a non-empty regular file. These checks are made on
// the open file rather than on its path so that the file that is checked is
// the file that is subsequently read.
base::File OpenSourceFile(const base::FilePath& source_file) {
  // Share the file for all access so that neither this open nor a concurrent
  // open by another process fails with a sharing violation. The path is chosen
  // by the caller and may name a pipe rather than a file, so open it with
  // SECURITY_ANONYMOUS to prevent the pipe's server from impersonating this
  // process. base::File does not support this, so the file is opened directly.
  base::File file(::CreateFileW(
      source_file.value().c_str(), /*dwDesiredAccess=*/GENERIC_READ,
      /*dwShareMode=*/FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      /*lpSecurityAttributes=*/nullptr, /*dwCreationDisposition=*/OPEN_EXISTING,
      /*dwFlagsAndAttributes=*/FILE_FLAG_SEQUENTIAL_SCAN |
          SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
      /*hTemplateFile=*/nullptr));
  if (!file.IsValid()) {
    PLOG(ERROR) << "Failed to open CRX";
    return base::File();
  }
  if (::GetFileType(file.GetPlatformFile()) != FILE_TYPE_DISK ||
      file.GetLength() <= 0) {
    LOG(ERROR) << "CRX is not a non-empty regular file";
    return base::File();
  }
  return file;
}

InstallStatus InstallComponentInternal(
    const base::FilePath& source_file,
    const InstallerState& installer_state,
    ComponentFactory component_factory,
    crx_file::VerifierFormat verifier_format) {
  // Source file must be absolute, non-empty and must not reference parent dirs.
  CHECK(source_file.IsAbsolute() && !source_file.ReferencesParent());

  // Open the file once and use only the open file from here on, so that the
  // file that is checked is the file that is staged.
  base::File source = OpenSourceFile(source_file);
  if (!source.IsValid()) {
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // The staged CRX is a copy of a caller-chosen file made before the file has
  // been validated. We stage it in a secure temporary directory (SystemTemp on
  // Windows when running as SYSTEM or default admin, or the users's private
  // Temp directory) to ensure unprivileged users cannot read back files they
  // cannot open themselves.
  base::ScopedTempDir staged_crx_dir;
  if (!staged_crx_dir.CreateUniqueTempDir(FILE_PATH_LITERAL("CrxStaging"))) {
    PLOG(ERROR) << "Failed to create CRX staging directory";
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  const base::FilePath staged_file =
      staged_crx_dir.GetPath().Append(kStagedCrxFilename);
  {
    base::File staged(staged_file, base::File::FLAG_CREATE |
                                       base::File::FLAG_WRITE |
                                       base::File::FLAG_WIN_SEQUENTIAL_SCAN);
    if (!staged.IsValid() || !base::CopyFileContents(source, staged)) {
      PLOG(ERROR) << "Failed to copy CRX to staging directory";
      return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
    }
  }
  // The staged copy is verified and unpacked by path, so neither file needs to
  // remain open.
  source.Close();

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

  std::optional<std::vector<uint8_t>> public_key_bytes =
      base::Base64Decode(public_key);
  if (!public_key_bytes.has_value()) {
    LOG(ERROR) << "Failed to decode CRX public key.";
    return installer::INSTALL_COMPONENT_FAILED_SIGNATURE;
  }

  const auto public_key_sha256 = crypto::hash::Sha256(*public_key_bytes);

  std::unique_ptr<ComponentInterface> component =
      component_factory(public_key_sha256);
  if (!component) {
    LOG(ERROR) << "Unsupported component CRX ID: " << crx_id;
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  // Temp directory for unpacking the CRX payload, under the installer's dir.
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

  // The staged copy is no longer needed once its contents have been extracted.
  if (!staged_crx_dir.Delete()) {
    VLOG(1) << "Failed to delete staged CRX directory";
  }

  // Deserialization fails with an appropriate error message if the manifest
  // does not exist.
  base::FilePath manifest_path = unpack_dir.Append(kManifestFilename);
  JSONFileValueDeserializer deserializer(manifest_path);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root || !root->is_dict()) {
    LOG(ERROR) << "Failed to parse manifest.json: " << error;
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  const base::DictValue& manifest_dict = root->GetDict();
  if (!component->ReadManifest(manifest_dict)) {
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  const std::string* manifest_version = manifest_dict.FindString("version");
  if (!manifest_version) {
    LOG(ERROR) << "Failed to find version in manifest.json";
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }
  // Require the version to be in canonical form (e.g., reject "1.02") so that
  // the name of the directory into which it is installed, which is derived
  // from the parsed version, matches the version in the manifest.
  const base::Version component_version(*manifest_version);
  if (!component_version.IsValid() ||
      component_version.GetString() != *manifest_version) {
    LOG(ERROR) << "Invalid version in manifest: " << *manifest_version;
    return installer::INSTALL_COMPONENT_INVALID_INPUT;
  }

  // Move the manifest out of the payload so that it can be copied into the
  // destination last. The presence of the manifest marks the installation as
  // complete and ready for use; if the process terminates while the payload is
  // being copied, the destination will lack the manifest and will be ignored
  // and eventually deleted. FileConductor retries on transient failures (e.g.,
  // a scanner holding the file open), and a non-lenient move fails unless the
  // manifest is removed from the payload.
  base::FilePath staged_manifest_dir;
  if (!base::CreateTemporaryDirInDir(temp_path.path(), L"Manifest",
                                     &staged_manifest_dir)) {
    PLOG(ERROR) << "Failed to create manifest staging subdirectory in "
                << temp_path.path();
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }
  const base::FilePath staged_manifest_path =
      staged_manifest_dir.Append(kManifestFilename);
  if (!file_conductor.MoveEntry(manifest_path, staged_manifest_path)) {
    PLOG(ERROR) << "Failed to move manifest.json out of unpacked component";
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // Determine component destination root.
  ASSIGN_OR_RETURN(
      const base::FilePath component_root,
      component->DetermineDestinationRoot(installer_state, crx_id));

  // Verify no newer version is already installed.
  const base::Version highest_version =
      FindHighestComponentVersion(component_root);
  if (highest_version.IsValid() && highest_version >= component_version) {
    LOG(INFO) << "Component is already installed at a same or higher version: "
              << highest_version << " >= " << component_version;
    return installer::INSTALL_COMPONENT_ALREADY_EXISTS;
  }

  // Derive fixed destination.
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

  if (!component->InitializeDestinationRoot(installer_state, component_root)) {
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // Delete the target directory if it exists to clean up any previous
  // installation attempts. Using FileConductor allows rollback on failure.
  if (!file_conductor.DeleteEntry(target_dir)) {
    PLOG(WARNING) << "Failed to delete existing target directory: "
                  << target_dir;
  }

  // Copy the payload (which no longer contains the manifest) to the versioned
  // destination, then copy the manifest in last to mark the installation as
  // complete. Copying rather than moving is essential: a move retains the
  // security descriptor that an item had in the temp directory, whereas a copy
  // creates new items that inherit their ACEs from the destination. This
  // ensures that the installed files get the access rights set on the
  // destination root (e.g., the protected DACL that InitializeDestinationRoot
  // applies to a dynamic patch's per-user directory). The unpacked files in the
  // temp directory are deleted along with it.
  if (!file_conductor.CopyEntry(unpack_dir, target_dir)) {
    PLOG(ERROR) << "Failed to copy component payload to " << target_dir;
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }
  if (!file_conductor.CopyEntry(staged_manifest_path,
                                target_dir.Append(kManifestFilename))) {
    PLOG(ERROR) << "Failed to finalize manifest.json in target directory: "
                << target_dir;
    return installer::INSTALL_COMPONENT_FAILED_INTERNAL;
  }

  // Success, cancel the automatic undo.
  std::move(undo_on_failure).Cancel();
  DeleteInvalidComponentDirectories(component_root, component_version);
  return installer::INSTALL_COMPONENT_SUCCESS;
}

}  // namespace

InstallStatus InstallComponent(const base::FilePath& source_file,
                               const InstallationState& original_state,
                               const InstallerState& installer_state,
                               const base::CommandLine& command_line) {
  auto factory_lookup =
      [&command_line, &original_state, &installer_state](
          const std::array<uint8_t, crypto::hash::kSha256Size>& hash)
      -> std::unique_ptr<ComponentInterface> {
    auto it = kSupportedComponents.find(hash);
    return it != kSupportedComponents.end()
               ? it->second(command_line, original_state, installer_state)
               : nullptr;
  };
  return InstallComponentInternal(
      source_file, installer_state, factory_lookup,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF);
}

InstallStatus InstallComponentForTesting(
    const base::FilePath& source_file,
    const InstallerState& installer_state,
    ComponentFactory component_factory,
    crx_file::VerifierFormat verifier_format) {
  return InstallComponentInternal(source_file, installer_state,
                                  component_factory, verifier_format);
}

base::Version GetComponentVersion(const base::FilePath& version_dir) {
  // Version directories are named with the canonical form of their version, so
  // reject any name that merely parses as one (e.g., "1.02").
  const std::string name = version_dir.BaseName().MaybeAsASCII();
  base::Version version(name);
  if (!version.IsValid() || version.GetString() != name ||
      !base::PathExists(version_dir.Append(kManifestFilename))) {
    return base::Version();
  }
  return version;
}

base::Version FindHighestComponentVersion(
    const base::FilePath& component_root) {
  base::Version highest_version;
  base::FileEnumerator(component_root, /*recursive=*/false,
                       base::FileEnumerator::DIRECTORIES)
      .ForEach([&highest_version](const base::FilePath& existing_dir) {
        base::Version version = GetComponentVersion(existing_dir);
        if (version.IsValid() &&
            (!highest_version.IsValid() || version > highest_version)) {
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
        const base::Version existing_version =
            GetComponentVersion(existing_dir);
        if (!existing_version.IsValid() || existing_version < keep_version) {
          if (base::DeletePathRecursively(existing_dir)) {
            VLOG(1) << "Deleted old or invalid component directory: "
                    << existing_dir;
          } else {
            PLOG(WARNING)
                << "Failed to delete old or invalid component directory "
                << existing_dir;
            // Scheduling deletion at reboot requires admin rights, so this
            // does nothing for user-level installs. That is acceptable: the
            // directory will be retried the next time a component is installed
            // into `component_root`.
            ScheduleDirectoryForDeletion(existing_dir);
          }
        }
      });
}

}  // namespace installer
