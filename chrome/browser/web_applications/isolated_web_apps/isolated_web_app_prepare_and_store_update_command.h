// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_PREPARE_AND_STORE_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_PREPARE_AND_STORE_UPDATE_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

struct IsolatedWebAppUpdatePrepareAndStoreCommandSuccess {
  IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
      base::Version update_version,
      IsolatedWebAppStorageLocation destination_location);
  IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
      const IsolatedWebAppUpdatePrepareAndStoreCommandSuccess& other);
  ~IsolatedWebAppUpdatePrepareAndStoreCommandSuccess();

  base::Version update_version;
  IsolatedWebAppStorageLocation location;
};

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdatePrepareAndStoreCommandSuccess& success);

struct IsolatedWebAppUpdatePrepareAndStoreCommandError {
  std::string message;
};

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdatePrepareAndStoreCommandError& error);

using IsolatedWebAppUpdatePrepareAndStoreCommandResult =
    base::expected<IsolatedWebAppUpdatePrepareAndStoreCommandSuccess,
                   IsolatedWebAppUpdatePrepareAndStoreCommandError>;

// This command prepares the update of an Isolated Web App by dry-running the
// update, and, on success, persisting the information about the pending update
// into the Web App database.
class IsolatedWebAppUpdatePrepareAndStoreCommand
    : public WebAppCommand<AppLock,
                           IsolatedWebAppUpdatePrepareAndStoreCommandResult> {
 public:
  class UpdateInfo {
   public:
    UpdateInfo(IwaSourceWithModeAndFileOp source,
               std::optional<base::Version> expected_version);
    ~UpdateInfo();

    UpdateInfo(const UpdateInfo&);
    UpdateInfo& operator=(const UpdateInfo&);

    base::Value AsDebugValue() const;

    const IwaSourceWithModeAndFileOp& source() const { return source_; }
    const std::optional<base::Version>& expected_version() const {
      return expected_version_;
    }

   private:
    IwaSourceWithModeAndFileOp source_;
    std::optional<base::Version> expected_version_;
  };

  // `update_info` specifies the location of the update for the IWA referred to
  // in `url_info`. This command is safe to run even if the IWA is not installed
  // or already updated, in which case it will gracefully fail. If a dry-run
  // of the update succeeds, then the `update_info` is persisted in the
  // `IsolationData::pending_update_info()` of the IWA in the Web App database.
  IsolatedWebAppUpdatePrepareAndStoreCommand(
      UpdateInfo update_info,
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<void(IsolatedWebAppUpdatePrepareAndStoreCommandResult)>
          callback,
      std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper);

  IsolatedWebAppUpdatePrepareAndStoreCommand(
      const IsolatedWebAppUpdatePrepareAndStoreCommand&) = delete;
  IsolatedWebAppUpdatePrepareAndStoreCommand& operator=(
      const IsolatedWebAppUpdatePrepareAndStoreCommand&) = delete;

  IsolatedWebAppUpdatePrepareAndStoreCommand(
      IsolatedWebAppUpdatePrepareAndStoreCommand&&) = delete;
  IsolatedWebAppUpdatePrepareAndStoreCommand& operator=(
      IsolatedWebAppUpdatePrepareAndStoreCommand&&) = delete;

  ~IsolatedWebAppUpdatePrepareAndStoreCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  using TrustCheckResult =
      base::expected<std::optional<web_package::SignedWebBundleIntegrityBlock>,
                     std::string>;

  void ReportFailure(std::string_view message);
  void ReportSuccess(const base::Version& update_version);

  Profile& profile();

  void CheckIfUpdateIsStillApplicable(base::OnceClosure next_step_callback);

  void CopyToProfileDirectory(base::OnceClosure next_step_callback);

  void OnCopiedToProfileDirectory(
      base::OnceClosure next_step_callback,
      base::expected<IsolatedWebAppStorageLocation, std::string> new_location);

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback);

  void OnTrustAndSignaturesChecked(base::OnceClosure next_step_callback,
                                   TrustCheckResult trust_check_result);

  void CreateStoragePartition(base::OnceClosure next_step_callback);

  void PrepareInstallInfo(
      base::OnceCallback<void(PrepareInstallInfoJob::InstallInfoOrFailure)>
          next_step_callback);

  void SetPendingUpdateInfo(PrepareInstallInfoJob::InstallInfoOrFailure result);

  void OnFinalized(const base::Version& update_version, bool success);

  std::unique_ptr<AppLock> lock_;

  const std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper_;

  const IsolatedWebAppUrlInfo url_info_;
  const std::optional<base::Version> expected_version_;

  // The inferred integrity block data of the update bundle being processed.
  std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data_;

  bool same_version_update_allowed_by_key_rotation_ = false;

  std::optional<IwaSourceWithModeAndFileOp> update_source_;
  std::optional<IwaSourceWithMode> destination_location_;
  std::optional<IsolatedWebAppStorageLocation> destination_storage_location_;
  std::optional<base::Version> installed_version_;

  std::unique_ptr<content::WebContents> web_contents_;

  const std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  const std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;

  std::unique_ptr<PrepareInstallInfoJob> prepare_install_info_job_;

  base::WeakPtrFactory<IsolatedWebAppUpdatePrepareAndStoreCommand>
      weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_PREPARE_AND_STORE_UPDATE_COMMAND_H_
