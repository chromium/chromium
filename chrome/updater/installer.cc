// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/updater/action_handler.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy_service.h"
#include "chrome/updater/util.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace updater {

namespace {

// This task joins a process, hence .WithBaseSyncPrimitives().
static constexpr base::TaskTraits kTaskTraitsBlockWithSyncPrimitives = {
    base::MayBlock(), base::WithBaseSyncPrimitives(),
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// Returns the full path to the installation directory for the application
// identified by the |app_id|.
base::FilePath GetAppInstallDir(const std::string& app_id) {
  base::FilePath app_install_dir;
  if (GetBaseDirectory(&app_install_dir)) {
    app_install_dir = app_install_dir.AppendASCII(kAppsDir);
    app_install_dir = app_install_dir.AppendASCII(app_id);
  }
  return app_install_dir;
}

}  // namespace

Installer::Installer(const std::string& app_id,
                     scoped_refptr<PersistedData> persisted_data)
    : app_id_(app_id), persisted_data_(persisted_data) {}

Installer::~Installer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

update_client::CrxComponent Installer::MakeCrxComponent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << __func__ << " for " << app_id_;

  // |pv| is the version of the registered app, persisted in prefs, and used
  // in the update checks and pings.
  const auto pv = persisted_data_->GetProductVersion(app_id_);
  if (pv.IsValid()) {
    pv_ = pv;
    checker_path_ = persisted_data_->GetExistenceCheckerPath(app_id_);
    fingerprint_ = persisted_data_->GetFingerprint(app_id_);
  } else {
    pv_ = base::Version(kNullVersion);
  }

  update_client::CrxComponent component;
  component.installer = scoped_refptr<Installer>(this);
  component.action_handler = MakeActionHandler();
  component.requires_network_encryption = false;
  component.crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
  component.app_id = app_id_;
  component.name = app_id_;
  component.version = pv_;
  component.fingerprint = fingerprint_;

  // In case we fail at getting the target channel, make sure that
  // |component.channel| is an empty string. Possible failure cases are if the
  // machine is not managed, the policy was not set or any other unexpected
  // error.
  if (!GetUpdaterPolicyService()->GetTargetChannel(app_id_, &component.channel))
    component.channel.clear();
  return component;
}

void Installer::DeleteOlderInstallPaths() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  const base::FilePath app_install_dir = GetAppInstallDir(app_id_);
  if (app_install_dir.empty() || !base::PathExists(app_install_dir)) {
    return;
  }

  base::FileEnumerator file_enumerator(app_install_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (auto path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    const base::Version version_dir(path.BaseName().MaybeAsASCII());

    // Mark for deletion any valid versioned directory except the directory
    // for the currently registered app.
    if (version_dir.IsValid() && version_dir.CompareTo(pv_)) {
      base::DeletePathRecursively(path);
    }
  }
}

Installer::Result Installer::InstallHelper(
    const base::FilePath& unpack_path,
    std::unique_ptr<InstallParams> install_params,
    ProgressCallback progress_callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  auto local_manifest = update_client::ReadManifest(unpack_path);
  if (!local_manifest)
    return Result(update_client::InstallError::BAD_MANIFEST);

  std::string version_ascii;
  local_manifest->GetStringASCII("version", &version_ascii);
  const base::Version manifest_version(version_ascii);

  VLOG(1) << "Installed version=" << pv_
          << ", installing version=" << manifest_version.GetString();
  if (!manifest_version.IsValid())
    return Result(update_client::InstallError::INVALID_VERSION);

  if (pv_.CompareTo(manifest_version) > 0)
    return Result(update_client::InstallError::VERSION_NOT_UPGRADED);

  const base::FilePath app_install_dir = GetAppInstallDir(app_id_);
  if (app_install_dir.empty())
    return Result(update_client::InstallError::NO_DIR_COMPONENT_USER);
  if (!base::CreateDirectory(app_install_dir))
    return Result(kErrorCreateAppInstallDirectory);

  const auto versioned_install_dir =
      app_install_dir.AppendASCII(manifest_version.GetString());
  if (base::PathExists(versioned_install_dir)) {
    if (!base::DeletePathRecursively(versioned_install_dir))
      return Result(update_client::InstallError::CLEAN_INSTALL_DIR_FAILED);
  }

  VLOG(1) << "Install_path=" << versioned_install_dir.AsUTF8Unsafe();

  // TODO(sorin): fix this once crbug.com/1042224 is resolved.
  // Moving the unpacked files to install this app is just a temporary fix
  // to make the prototype code work end to end, until app registration for
  // updates is implemented.
  if (!base::Move(unpack_path, versioned_install_dir)) {
    PLOG(ERROR) << "Move failed.";
    base::DeletePathRecursively(versioned_install_dir);
    return Result(update_client::InstallError::MOVE_FILES_ERROR);
  }

  DCHECK(!base::PathExists(unpack_path));
  DCHECK(base::PathExists(versioned_install_dir));

  // Resolve the path to an installer file, which is included in the CRX, and
  // specified by the |run| attribute in the manifest object of an update
  // response.
  if (!install_params || install_params->run.empty())
    return Result(kErrorMissingInstallParams);

  // Assume the install params are ASCII for now.
  const auto application_installer =
      versioned_install_dir.AppendASCII(install_params->run);
  if (!base::PathExists(application_installer))
    return Result(kErrorMissingRunableFile);

  // TODO(crbug.com/1014630): handle the installer API.
  const int exit_code =
      RunApplicationInstaller(application_installer, install_params->arguments,
                              std::move(progress_callback));

  // Upon success, when the control flow returns back to the |update_client|,
  // the prefs are updated asynchronously with the new |pv| and |fingerprint|.
  // The task sequencing guarantees that the prefs will be updated by the
  // time another CrxDataCallback is invoked, which needs updated values.
  return exit_code == 0 ? Result(update_client::InstallError::NONE)
                        : Result(kErrorApplicationInstallerFailed, exit_code);
}

void Installer::InstallWithSyncPrimitives(
    const base::FilePath& unpack_path,
    const std::string& public_key,
    std::unique_ptr<InstallParams> install_params,
    ProgressCallback progress_callback,
    Callback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  DeleteOlderInstallPaths();
  const auto result = InstallHelper(unpack_path, std::move(install_params),
                                    std::move(progress_callback));
  base::DeletePathRecursively(unpack_path);
  std::move(callback).Run(result);
}

void Installer::OnUpdateError(int error) {
  LOG(ERROR) << "updater error: " << error << " for " << app_id_;
}

void Installer::Install(const base::FilePath& unpack_path,
                        const std::string& public_key,
                        std::unique_ptr<InstallParams> install_params,
                        ProgressCallback progress_callback,
                        Callback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, kTaskTraitsBlockWithSyncPrimitives,
      base::BindOnce(&Installer::InstallWithSyncPrimitives, this, unpack_path,
                     public_key, std::move(install_params),
                     std::move(progress_callback), std::move(callback)));
}

bool Installer::GetInstalledFile(const std::string& file,
                                 base::FilePath* installed_file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (pv_ == base::Version(kNullVersion))
    return false;  // No component has been installed yet.

  const auto install_dir = GetCurrentInstallDir();
  if (install_dir.empty())
    return false;

  *installed_file = install_dir.AppendASCII(file);
  return true;
}

bool Installer::Uninstall() {
  return false;
}

base::FilePath Installer::GetCurrentInstallDir() const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  return GetAppInstallDir(app_id_).AppendASCII(pv_.GetString());
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
int Installer::RunApplicationInstaller(const base::FilePath& app_installer,
                                       const std::string& arguments) {
  NOTREACHED();
  return -1;
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // namespace updater
