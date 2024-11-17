// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_APPLY_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_APPLY_UPDATE_COMMAND_H_

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
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace content {
class WebContents;
}

namespace webapps {
enum class InstallResultCode;
}

namespace web_app {

struct IsolatedWebAppApplyUpdateCommandError {
  std::string message;
};

std::ostream& operator<<(std::ostream& os,
                         const IsolatedWebAppApplyUpdateCommandError& error);

// This command applies a pending update of an Isolated Web App. Information
// about the pending update is read from
// `IsolationData::pending_update_info`. Both on success, and on
// failure, the pending update info is removed from the Web App database.
class IsolatedWebAppApplyUpdateCommand
    : public WebAppCommand<
          AppLock,
          base::expected<void, IsolatedWebAppApplyUpdateCommandError>> {
 public:
  // This command is safe to run even if the IWA is not installed or already
  // updated, in which case it will gracefully fail.
  IsolatedWebAppApplyUpdateCommand(
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<
          void(base::expected<void, IsolatedWebAppApplyUpdateCommandError>)>
          callback,
      std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper);

  IsolatedWebAppApplyUpdateCommand(const IsolatedWebAppApplyUpdateCommand&) =
      delete;
  IsolatedWebAppApplyUpdateCommand& operator=(
      const IsolatedWebAppApplyUpdateCommand&) = delete;

  IsolatedWebAppApplyUpdateCommand(IsolatedWebAppApplyUpdateCommand&&) = delete;
  IsolatedWebAppApplyUpdateCommand& operator=(
      IsolatedWebAppApplyUpdateCommand&&) = delete;

  ~IsolatedWebAppApplyUpdateCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  using TrustCheckResult = base::expected<void, std::string>;

  void ReportFailure(std::string_view message);
  void ReportSuccess();

  Profile& profile();

  void CheckIfUpdateIsStillPending(base::OnceClosure next_step_callback);

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback);

  void OnTrustAndSignaturesChecked(base::OnceClosure next_step_callback,
                                   TrustCheckResult trust_check_result);

  void HandleKeyRotationIfNecessary(base::OnceClosure next_step_callback);

  void CreateStoragePartition(base::OnceClosure next_step_callback);

  void PrepareInstallInfo(
      base::OnceCallback<void(PrepareInstallInfoJob::InstallInfoOrFailure)>
          next_step_callback);

  void FinalizeUpdate(PrepareInstallInfoJob::InstallInfoOrFailure result);

  void OnFinalized(const webapps::AppId& app_id,
                   webapps::InstallResultCode update_result_code);

  void CleanupOnFailure(base::OnceClosure next_step_callback);

  // These fields are set in `CheckIfUpdateIsStillPending()`. Calling them
  // before that will trigger a CHECK-fail.
  const IsolationData& isolation_data() const { return *isolation_data_; }
  const IsolationData::PendingUpdateInfo pending_update_info() const {
    return *isolation_data_->pending_update_info();
  }

  std::unique_ptr<AppLock> lock_;

  const IsolatedWebAppUrlInfo url_info_;

  std::unique_ptr<content::WebContents> web_contents_;

  const std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  const std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;

  std::optional<IsolationData> isolation_data_;

  const std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper_;

  std::unique_ptr<PrepareInstallInfoJob> prepare_install_info_job_;

  base::WeakPtrFactory<IsolatedWebAppApplyUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_APPLY_UPDATE_COMMAND_H_
