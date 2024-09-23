// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_FINALIZE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_FINALIZE_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/common/web_app_id.h"

namespace webapps {
enum class InstallResultCode;
}  // namespace webapps

namespace web_app {

// After all app windows have closed, this command runs to perform the last few
// steps of writing the data to the DB.
class ManifestUpdateFinalizeCommand
    : public WebAppCommand<AppLock,
                           const GURL&,
                           const webapps::AppId&,
                           ManifestUpdateResult> {
 public:
  using ManifestWriteCallback =
      base::OnceCallback<void(const GURL& url,
                              const webapps::AppId& app_id,
                              ManifestUpdateResult result)>;

  ManifestUpdateFinalizeCommand(
      const GURL& url,
      const webapps::AppId& app_id,
      std::unique_ptr<WebAppInstallInfo> install_info,
      ManifestWriteCallback write_callback,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive);

  ~ManifestUpdateFinalizeCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

  base::WeakPtr<ManifestUpdateFinalizeCommand> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnInstallationComplete(const webapps::AppId& app_id,
                              webapps::InstallResultCode code);
  void CompleteCommand(webapps::InstallResultCode code,
                       ManifestUpdateResult result);

  std::unique_ptr<AppLock> lock_;

  const GURL url_;
  const webapps::AppId app_id_;
  std::unique_ptr<WebAppInstallInfo> install_info_;
  // Two KeepAlive objects, to make sure that manifest update writes
  // still happen even though the app window has closed.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  base::WeakPtrFactory<ManifestUpdateFinalizeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_FINALIZE_COMMAND_H_
