// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_APPLY_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_APPLY_UPDATE_COMMAND_H_

#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace webapps {
enum class InstallResultCode;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class WebAppUrlLoader;

enum class WebAppUrlLoaderResult;

struct IsolatedWebAppApplyUpdateCommandError {
  std::string message;

  friend std::ostream& operator<<(
      std::ostream& os,
      const IsolatedWebAppApplyUpdateCommandError& error) {
    return os << "IsolatedWebAppApplyUpdateCommandError { "
                 "message = \""
              << error.message << "\" }.";
  }
};

// This command applies a pending update of an Isolated Web App. Information
// about the pending update is read from
// `WebApp::IsolationData::pending_update_info`. Both on success, and on
// failure, the pending update info is removed from the Web App database.
class IsolatedWebAppApplyUpdateCommand : public WebAppCommandTemplate<AppLock> {
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

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  void OnShutdown() override;

 private:
  void ReportFailure(base::StringPiece message, bool due_to_shutdown = false);
  void ReportSuccess();

  template <typename T, std::enable_if_t<std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceClosure next_step_callback,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      ReportFailure(status.error());
    } else {
      std::move(next_step_callback).Run();
    }
  }

  template <typename T, std::enable_if_t<!std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceCallback<void(T)> next_step_callback,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      ReportFailure(status.error());
    } else {
      std::move(next_step_callback).Run(std::move(*status));
    }
  }

  Profile& profile();

  const WebApp::IsolationData::PendingUpdateInfo& update_info() const {
    return *installed_app_->isolation_data()->pending_update_info();
  }

  void CheckIfUpdateIsStillPending(base::OnceClosure next_step_callback);

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback);

  void CreateStoragePartition(base::OnceClosure next_step_callback);

  void LoadInstallUrl(base::OnceClosure next_step_callback);

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<
          void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
          next_step_callback);

  void ValidateManifestAndCreateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url);

  void RetrieveIconsAndPopulateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      WebAppInstallInfo install_info);

  void Finalize(WebAppInstallInfo info);

  void OnFinalized(const webapps::AppId& app_id,
                   webapps::InstallResultCode update_result_code,
                   OsHooksErrors os_hooks_errors);

  void CleanupUpdateInfoOnFailure(base::OnceClosure next_step_callback);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;
  base::Value::Dict debug_log_;

  IsolatedWebAppUrlInfo url_info_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;

  raw_ptr<const WebApp> installed_app_ = nullptr;

  base::OnceCallback<void(
      base::expected<void, IsolatedWebAppApplyUpdateCommandError>)>
      callback_;

  std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper_;

  base::WeakPtrFactory<IsolatedWebAppApplyUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_APPLY_UPDATE_COMMAND_H_
