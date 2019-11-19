// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace component_updater {

namespace {

// Version "0" corresponds to no installed version. By the server's conventions,
// we represent it as a dotted quad.
const char kNullVersion[] = "0.0.0.0";

using Result = update_client::CrxInstaller::Result;
using InstallError = update_client::InstallError;

}  // namespace

ComponentInstallerPolicy::~ComponentInstallerPolicy() {}

ComponentInstaller::RegistrationInfo::RegistrationInfo()
    : version(kNullVersion) {}

ComponentInstaller::RegistrationInfo::~RegistrationInfo() = default;

ComponentInstaller::ComponentInstaller(
    std::unique_ptr<ComponentInstallerPolicy> installer_policy)
    : current_version_(kNullVersion),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  installer_policy_ = std::move(installer_policy);
}

ComponentInstaller::~ComponentInstaller() {}

void ComponentInstaller::Register(ComponentUpdateService* cus,
                                  base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Some components may affect user visible features, hence USER_VISIBLE.
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  if (!installer_policy_) {
    LOG(ERROR) << "A ComponentInstaller has been created but "
               << "has no installer policy.";
    return;
  }

  auto registration_info = base::MakeRefCounted<RegistrationInfo>();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ComponentInstaller::StartRegistration, this,
                     registration_info),
      base::BindOnce(&ComponentInstaller::FinishRegistration, this,
                     registration_info, cus, std::move(callback)));
}

void ComponentInstaller::OnUpdateError(int error) {
  LOG(ERROR) << "Component update error: " << error;
}

Result ComponentInstaller::InstallHelper(
    const base::FilePath& unpack_path,
    std::unique_ptr<base::DictionaryValue>* manifest,
    base::Version* version,
    base::FilePath* install_path) {
  auto local_manifest = update_client::ReadManifest(unpack_path);
  if (!local_manifest)
    return Result(InstallError::BAD_MANIFEST);

  std::string version_ascii;
  local_manifest->GetStringASCII("version", &version_ascii);
  const base::Version manifest_version(version_ascii);

  VLOG(1) << "Install: version=" << manifest_version.GetString()
          << " current version=" << current_version_.GetString();

  if (!manifest_version.IsValid())
    return Result(InstallError::INVALID_VERSION);
  if (current_version_.CompareTo(manifest_version) > 0)
    return Result(InstallError::VERSION_NOT_UPGRADED);
  base::FilePath local_install_path;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &local_install_path))
    return Result(InstallError::NO_DIR_COMPONENT_USER);
  local_install_path =
      local_install_path.Append(installer_policy_->GetRelativeInstallDir())
          .AppendASCII(manifest_version.GetString());
  if (base::PathExists(local_install_path)) {
    if (!base::DeleteFileRecursively(local_install_path))
      return Result(InstallError::CLEAN_INSTALL_DIR_FAILED);
  }

  VLOG(1) << "unpack_path=" << unpack_path.AsUTF8Unsafe()
          << " install_path=" << local_install_path.AsUTF8Unsafe();

  if (!base::Move(unpack_path, local_install_path)) {
    PLOG(ERROR) << "Move failed.";
    base::DeleteFileRecursively(local_install_path);
    return Result(InstallError::MOVE_FILES_ERROR);
  }

  // Acquire the ownership of the |local_install_path|.
  base::ScopedTempDir install_path_owner;
  ignore_result(install_path_owner.Set(local_install_path));

#if defined(OS_CHROMEOS)
  if (!base::SetPosixFilePermissions(local_install_path, 0755)) {
    PLOG(ERROR) << "SetPosixFilePermissions failed: "
                << local_install_path.value();
    return Result(InstallError::SET_PERMISSIONS_FAILED);
  }
#endif  // defined(OS_CHROMEOS)

  DCHECK(!base::PathExists(unpack_path));
  DCHECK(base::PathExists(local_install_path));

  const Result result =
      installer_policy_->OnCustomInstall(*local_manifest, local_install_path);
  if (result.error)
    return result;

  if (!installer_policy_->VerifyInstallation(*local_manifest,
                                             local_install_path))
    return Result(InstallError::INSTALL_VERIFICATION_FAILED);

  *manifest = std::move(local_manifest);
  *version = manifest_version;
  *install_path = install_path_owner.Take();

  return Result(InstallError::NONE);
}

void ComponentInstaller::Install(const base::FilePath& unpack_path,
                                 const std::string& /*public_key*/,
                                 Callback callback) {
  std::unique_ptr<base::DictionaryValue> manifest;
  base::Version version;
  base::FilePath install_path;
  const Result result =
      InstallHelper(unpack_path, &manifest, &version, &install_path);
  base::DeleteFileRecursively(unpack_path);
  if (result.error) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), result));
    return;
  }

  current_version_ = version;
  current_install_dir_ = install_path;

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ComponentInstaller::ComponentReady, this,
                                std::move(manifest)));
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
}

bool ComponentInstaller::GetInstalledFile(const std::string& file,
                                          base::FilePath* installed_file) {
  if (current_version_ == base::Version(kNullVersion))
    return false;  // No component has been installed yet.
  *installed_file = current_install_dir_.AppendASCII(file);
  return true;
}

bool ComponentInstaller::Uninstall() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ComponentInstaller::UninstallOnTaskRunner, this));
  return true;
}

bool ComponentInstaller::FindPreinstallation(
    const base::FilePath& root,
    scoped_refptr<RegistrationInfo> registration_info) {
  base::FilePath path = root.Append(installer_policy_->GetRelativeInstallDir());
  if (!base::PathExists(path)) {
    DVLOG(1) << "Relative install dir does not exist: " << path.MaybeAsASCII();
    return false;
  }

  std::unique_ptr<base::DictionaryValue> manifest =
      update_client::ReadManifest(path);
  if (!manifest) {
    DVLOG(1) << "Manifest does not exist: " << path.MaybeAsASCII();
    return false;
  }

  if (!installer_policy_->VerifyInstallation(*manifest, path)) {
    DVLOG(1) << "Installation verification failed: " << path.MaybeAsASCII();
    return false;
  }

  std::string version_lexical;
  if (!manifest->GetStringASCII("version", &version_lexical)) {
    DVLOG(1) << "Failed to get component version from the manifest.";
    return false;
  }

  const base::Version version(version_lexical);
  if (!version.IsValid()) {
    DVLOG(1) << "Version in the manifest is invalid:" << version_lexical;
    return false;
  }

  VLOG(1) << "Preinstalled component found for " << installer_policy_->GetName()
          << " at " << path.MaybeAsASCII() << " with version " << version
          << ".";

  registration_info->install_dir = path;
  registration_info->version = version;
  registration_info->manifest = std::move(manifest);

  return true;
}

void ComponentInstaller::StartRegistration(
    scoped_refptr<RegistrationInfo> registration_info) {
  VLOG(1) << __func__ << " for " << installer_policy_->GetName();
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::Version latest_version(kNullVersion);

  // First check for an installation set up alongside Chrome itself.
  base::FilePath root;
  if (base::PathService::Get(DIR_COMPONENT_PREINSTALLED, &root) &&
      FindPreinstallation(root, registration_info)) {
    latest_version = registration_info->version;
  }

  // If there is a distinct alternate root, check there as well, and override
  // anything found in the basic root.
  base::FilePath root_alternate;
  if (base::PathService::Get(DIR_COMPONENT_PREINSTALLED_ALT, &root_alternate) &&
      root != root_alternate &&
      FindPreinstallation(root_alternate, registration_info)) {
    latest_version = registration_info->version;
  }

  // Then check for a higher-versioned user-wide installation.
  base::FilePath latest_path;
  std::unique_ptr<base::DictionaryValue> latest_manifest;
  base::FilePath base_component_dir;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &base_component_dir))
    return;
  base::FilePath base_dir =
      base_component_dir.Append(installer_policy_->GetRelativeInstallDir());
  if (!base::PathExists(base_dir) && !base::CreateDirectory(base_dir)) {
    PLOG(ERROR) << "Could not create the base directory for "
                << installer_policy_->GetName() << " ("
                << base_dir.MaybeAsASCII() << ").";
    return;
  }

#if defined(OS_CHROMEOS)
  base::FilePath base_dir_ = base_component_dir;
  std::vector<base::FilePath::StringType> components;
  installer_policy_->GetRelativeInstallDir().GetComponents(&components);
  for (const base::FilePath::StringType component : components) {
    base_dir_ = base_dir_.Append(component);
    if (!base::SetPosixFilePermissions(base_dir_, 0755)) {
      PLOG(ERROR) << "SetPosixFilePermissions failed: " << base_dir.value();
      return;
    }
  }
#endif  // defined(OS_CHROMEOS)

  std::vector<base::FilePath> older_paths;
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by component installer so do not try to remove them.
    if (!version.IsValid())
      continue;

    // |version| not newer than the latest found version (kNullVersion if no
    // version has been found yet) is marked for removal.
    if (version.CompareTo(latest_version) <= 0) {
      older_paths.push_back(path);
      continue;
    }

    std::unique_ptr<base::DictionaryValue> manifest =
        update_client::ReadManifest(path);
    if (!manifest || !installer_policy_->VerifyInstallation(*manifest, path)) {
      PLOG(ERROR) << "Failed to read manifest or verify installation for "
                  << installer_policy_->GetName() << " (" << path.MaybeAsASCII()
                  << ").";
      older_paths.push_back(path);
      continue;
    }

    // New valid |version| folder found!

    if (!latest_path.empty())
      older_paths.push_back(latest_path);

    latest_path = path;
    latest_version = version;
    latest_manifest = std::move(manifest);
  }

  if (latest_manifest) {
    registration_info->version = latest_version;
    registration_info->manifest = std::move(latest_manifest);
    registration_info->install_dir = latest_path;
    base::ReadFileToString(latest_path.AppendASCII("manifest.fingerprint"),
                           &registration_info->fingerprint);
  }

  // Remove older versions of the component. None should be in use during
  // browser startup.
  for (const auto& older_path : older_paths)
    base::DeleteFileRecursively(older_path);
}

void ComponentInstaller::UninstallOnTaskRunner() {
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Only try to delete any files that are in our user-level install path.
  base::FilePath userInstallPath;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &userInstallPath))
    return;
  if (!userInstallPath.IsParent(current_install_dir_))
    return;

  const base::FilePath base_dir = current_install_dir_.DirName();
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by the component installer, so do not try to remove them.
    if (!version.IsValid())
      continue;

    if (!base::DeleteFileRecursively(path))
      DLOG(ERROR) << "Couldn't delete " << path.value();
  }

  // Delete the base directory if it's empty now.
  if (base::IsDirectoryEmpty(base_dir)) {
    if (!base::DeleteFile(base_dir, false))
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
  }

  // Customized operations for individual component.
  installer_policy_->OnCustomUninstall();
}

void ComponentInstaller::FinishRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    ComponentUpdateService* cus,
    base::OnceClosure callback) {
  VLOG(1) << __func__ << " for " << installer_policy_->GetName();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  current_install_dir_ = registration_info->install_dir;
  current_version_ = registration_info->version;
  current_fingerprint_ = registration_info->fingerprint;

  update_client::CrxComponent crx;
  installer_policy_->GetHash(&crx.pk_hash);
  crx.app_id = update_client::GetCrxIdFromPublicKeyHash(crx.pk_hash);
  crx.installer = this;
  crx.version = current_version_;
  crx.fingerprint = current_fingerprint_;
  crx.name = installer_policy_->GetName();
  crx.installer_attributes = installer_policy_->GetInstallerAttributes();
  crx.requires_network_encryption =
      installer_policy_->RequiresNetworkEncryption();
  crx.crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
  crx.handled_mime_types = installer_policy_->GetMimeTypes();
  crx.supports_group_policy_enable_component_updates =
      installer_policy_->SupportsGroupPolicyEnabledComponentUpdates();

  if (!cus->RegisterComponent(crx)) {
    LOG(ERROR) << "Component registration failed for "
               << installer_policy_->GetName();
    return;
  }

  if (registration_info->manifest) {
    ComponentReady(std::move(registration_info->manifest));
  } else {
    DVLOG(1) << "No component found for " << installer_policy_->GetName();
  }

  if (!callback.is_null())
    std::move(callback).Run();
}

void ComponentInstaller::ComponentReady(
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << current_version_.GetString()
          << " in " << current_install_dir_.value();
  installer_policy_->ComponentReady(current_version_, current_install_dir_,
                                    std::move(manifest));
}

}  // namespace component_updater
