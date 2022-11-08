// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profiles_state.h"

namespace {

base::File::Error CreateNonExistingDirectory(const base::FilePath& path) {
  if (base::PathExists(path)) {
    return base::File::FILE_ERROR_EXISTS;
  }
  base::File::Error err = base::File::FILE_OK;
  base::CreateDirectoryAndGetError(path, &err);
  return err;
}

}  // namespace

namespace web_app {

IsolatedWebAppPolicyManager::IsolatedWebAppPolicyManager(
    const base::FilePath& context_dir,
    std::vector<IsolatedWebAppExternalInstallOptions>&& iwa_install_options,
    base::OnceCallback<void(EphemeralAppInstallResult)> ephemeral_install_cb)
    : ephemeral_iwa_install_options_(std::move(iwa_install_options)),
      installation_dir_(context_dir.Append(kEphemeralIwaRootDirectory)),
      ephemeral_install_cb_(std::move(ephemeral_install_cb)) {}
IsolatedWebAppPolicyManager::~IsolatedWebAppPolicyManager() = default;

void IsolatedWebAppPolicyManager::InstallEphemeralApps() {
  if (!profiles::IsPublicSession()) {
    LOG(ERROR) << "The IWAs should be installed only in managed guest session.";
    std::move(ephemeral_install_cb_)
        .Run(EphemeralAppInstallResult::kErrorNotEphemeralSession);
    return;
  }

  if (ephemeral_iwa_install_options_.empty()) {
    std::move(ephemeral_install_cb_).Run(EphemeralAppInstallResult::kSuccess);
    return;
  }

  CreateIwaEphemeralRootDirectory();
}

void IsolatedWebAppPolicyManager::CreateIwaEphemeralRootDirectory() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CreateNonExistingDirectory, installation_dir_),
      base::BindOnce(
          &IsolatedWebAppPolicyManager::OnIwaEphemeralRootDirectoryCreated,
          weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnIwaEphemeralRootDirectoryCreated(
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Error in creating the directory for ephemeral IWAs: "
               << base::File::ErrorToString(error);
    std::move(ephemeral_install_cb_)
        .Run(EphemeralAppInstallResult::kErrorCantCreateRootDirectory);
    return;
  }

  LOG(ERROR) << "Next step would be downloading of the update manifest, but it "
                "is not yet implemented";
  std::move(ephemeral_install_cb_).Run(EphemeralAppInstallResult::kSuccess);
}

}  // namespace web_app
