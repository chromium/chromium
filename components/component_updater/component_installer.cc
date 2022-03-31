// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_installer.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/backup_util.h"
#endif

namespace component_updater {

namespace {

// Version "0" corresponds to no installed version. By the server's conventions,
// we represent it as a dotted quad.
const char kNullVersion[] = "0.0.0.0";

using Result = update_client::CrxInstaller::Result;
using InstallError = update_client::InstallError;

}  // namespace

ComponentInstallerPolicy::~ComponentInstallerPolicy() = default;

ComponentInstaller::RegistrationInfo::RegistrationInfo()
    : version(kNullVersion) {}

ComponentInstaller::RegistrationInfo::~RegistrationInfo() = default;

ComponentInstaller::ComponentInstaller(
    std::unique_ptr<ComponentInstallerPolicy> installer_policy,
    scoped_refptr<update_client::ActionHandler> action_handler)
    : current_version_(kNullVersion),
      installer_policy_(std::move(installer_policy)),
      action_handler_(action_handler),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

ComponentInstaller::~ComponentInstaller() = default;

void ComponentInstaller::Register(ComponentUpdateService* cus,
                                  base::OnceClosure callback,
                                  base::TaskPriority task_priority) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cus);
  Register(base::BindOnce(&ComponentUpdateService::RegisterComponent,
                          base::Unretained(cus)),
           std::move(callback), task_priority);
}

void ComponentInstaller::Register(RegisterCallback register_callback,
                                  base::OnceClosure callback,
                                  base::TaskPriority task_priority) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), task_priority,
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
                     registration_info, std::move(register_callback),
                     std::move(callback)));
}

void ComponentInstaller::OnUpdateError(int error) {
  LOG(ERROR) << "Component update error: " << error;
}

Result ComponentInstaller::InstallHelper(const base::FilePath& unpack_path,
                                         base::Value* manifest,
                                         base::Version* version,
                                         base::FilePath* install_path) {
  base::Value local_manifest = update_client::ReadManifest(unpack_path);
  if (!local_manifest.is_dict())
    return Result(InstallError::BAD_MANIFEST);

  const std::string* version_ascii = local_manifest.FindStringKey("version");
  if (!version_ascii || !base::IsStringASCII(*version_ascii))
    return Result(InstallError::INVALID_VERSION);

  const base::Version manifest_version(*version_ascii);

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
    if (!base::DeletePathRecursively(local_install_path))
      return Result(InstallError::CLEAN_INSTALL_DIR_FAILED);
  }

  VLOG(1) << "unpack_path=" << unpack_path.AsUTF8Unsafe()
          << " install_path=" << local_install_path.AsUTF8Unsafe();

  if (!base::Move(unpack_path, local_install_path)) {
    PLOG(ERROR) << "Move failed.";
    base::DeletePathRecursively(local_install_path);
    return Result(InstallError::MOVE_FILES_ERROR);
  }

  // Acquire the ownership of the |local_install_path|.
  base::ScopedTempDir install_path_owner;
  std::ignore = install_path_owner.Set(local_install_path);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::SetPosixFilePermissions(local_install_path, 0755)) {
    PLOG(ERROR) << "SetPosixFilePermissions failed: "
                << local_install_path.value();
    return Result(InstallError::SET_PERMISSIONS_FAILED);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DCHECK(!base::PathExists(unpack_path));
  DCHECK(base::PathExists(local_install_path));

#if BUILDFLAG(IS_APPLE)
  // Since components can be large and can be re-downloaded when needed, they
  // are excluded from backups.
  base::mac::SetBackupExclusion(local_install_path);
#endif

  const Result result =
      installer_policy_->OnCustomInstall(local_manifest, local_install_path);
  if (result.error)
    return result;

  if (!installer_policy_->VerifyInstallation(local_manifest,
                                             local_install_path)) {
    return Result(InstallError::INSTALL_VERIFICATION_FAILED);
  }

  *manifest = std::move(local_manifest);
  *version = manifest_version;
  *install_path = install_path_owner.Take();

  return Result(InstallError::NONE);
}

void ComponentInstaller::Install(
    const base::FilePath& unpack_path,
    const std::string& /*public_key*/,
    std::unique_ptr<InstallParams> /*install_params*/,
    ProgressCallback /*progress_callback*/,
    Callback callback) {
  base::Value manifest;
  base::Version version;
  base::FilePath install_path;
  const Result result =
      InstallHelper(unpack_path, &manifest, &version, &install_path);
  base::DeletePathRecursively(unpack_path);
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

  base::Value manifest = update_client::ReadManifest(path);
  if (!manifest.is_dict()) {
    DVLOG(1) << "Manifest does not exist: " << path.MaybeAsASCII();
    return false;
  }

  if (!installer_policy_->VerifyInstallation(manifest, path)) {
    DVLOG(1) << "Installation verification failed: " << path.MaybeAsASCII();
    return false;
  }

  std::string* version_lexical = manifest.FindStringKey("version");
  if (!version_lexical || !base::IsStringASCII(*version_lexical)) {
    DVLOG(1) << "Failed to get component version from the manifest.";
    return false;
  }

  const base::Version version(*version_lexical);
  if (!version.IsValid()) {
    DVLOG(1) << "Version in the manifest is invalid:" << *version_lexical;
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
  absl::optional<base::Value> latest_manifest;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath base_dir_ = base_component_dir;
  for (const base::FilePath::StringType& component :
       installer_policy_->GetRelativeInstallDir().GetComponents()) {
    base_dir_ = base_dir_.Append(component);
    if (!base::SetPosixFilePermissions(base_dir_, 0755)) {
      PLOG(ERROR) << "SetPosixFilePermissions failed: " << base_dir.value();
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

    base::Value manifest = update_client::ReadManifest(path);
    if (!manifest.is_dict() ||
        !installer_policy_->VerifyInstallation(manifest, path)) {
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
    registration_info->manifest = std::move(*latest_manifest);
    registration_info->install_dir = latest_path;
    base::ReadFileToString(latest_path.AppendASCII("manifest.fingerprint"),
                           &registration_info->fingerprint);
  }

  // Remove older versions of the component. None should be in use during
  // browser startup.
  for (const auto& older_path : older_paths)
    base::DeletePathRecursively(older_path);
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

    if (!base::DeletePathRecursively(path))
      DLOG(ERROR) << "Couldn't delete " << path.value();
  }

  // Delete the base directory if it's empty now.
  if (base::IsDirectoryEmpty(base_dir)) {
    if (!base::DeleteFile(base_dir))
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
  }

  // Customized operations for individual component.
  installer_policy_->OnCustomUninstall();
}

void ComponentInstaller::FinishRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    RegisterCallback register_callback,
    base::OnceClosure callback) {
  VLOG(1) << __func__ << " for " << installer_policy_->GetName();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  current_install_dir_ = registration_info->install_dir;
  current_version_ = registration_info->version;
  current_fingerprint_ = registration_info->fingerprint;

  std::vector<uint8_t> public_key_hash;
  installer_policy_->GetHash(&public_key_hash);

  if (!std::move(register_callback)
           .Run(ComponentRegistration(
               update_client::GetCrxIdFromPublicKeyHash(public_key_hash),
               installer_policy_->GetName(), public_key_hash, current_version_,
               current_fingerprint_,
               installer_policy_->GetInstallerAttributes(), action_handler_,
               this, installer_policy_->RequiresNetworkEncryption(),
               installer_policy_
                   ->SupportsGroupPolicyEnabledComponentUpdates()))) {
    LOG(ERROR) << "Component registration failed for "
               << installer_policy_->GetName();
    if (!callback.is_null())
      std::move(callback).Run();
    return;
  }

  if (registration_info->manifest) {
    ComponentReady(std::move(*registration_info->manifest));
  } else {
    DVLOG(1) << "No component found for " << installer_policy_->GetName();
  }

  if (!callback.is_null())
    std::move(callback).Run();
}

void ComponentInstaller::ComponentReady(base::Value manifest) {
  VLOG(1) << "Component ready, version " << current_version_.GetString()
          << " in " << current_install_dir_.value();
  installer_policy_->ComponentReady(current_version_, current_install_dir_,
                                    std::move(manifest));
}

}  // namespace component_updater
