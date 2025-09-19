// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_PENDING_MANIFEST_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_PENDING_MANIFEST_UPDATE_COMMAND_H_

#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "url/gurl.h"

namespace web_app {

// This enum is recorded by UMA, the numeric values must not change.
enum class ApplyPendingManifestUpdateResult {
  kSystemShutdown = 0,
  kAppNotInstalled = 1,
  kIconChangeAppliedSuccessfully = 2,
  kFailedToOverwriteIconsFromPendingIcons = 3,
  kNoPendingUpdate = 4,
  kMaxValue = kNoPendingUpdate
};

std::ostream& operator<<(std::ostream& os,
                         ApplyPendingManifestUpdateResult stage);

class ApplyPendingManifestUpdateCommand
    : public WebAppCommand<AppLock, ApplyPendingManifestUpdateResult> {
 public:
  using PassKey = base::PassKey<ApplyPendingManifestUpdateCommand>;
  using CompletedCallback =
      base::OnceCallback<void(ApplyPendingManifestUpdateResult update_result)>;
  ApplyPendingManifestUpdateCommand(const webapps::AppId& app_id,
                                    CompletedCallback callback);

  ~ApplyPendingManifestUpdateCommand() override;

 protected:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  // Sets the pending trusted icon and pending manifest icon metadata to the web
  // app.
  void ApplyPendingIconToWebApp(bool success);

  void CompleteCommandAndSelfDestruct(
      ApplyPendingManifestUpdateResult check_result);

  base::WeakPtr<ApplyPendingManifestUpdateCommand> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::unique_ptr<AppLock> lock_;
  const webapps::AppId app_id_;
  proto::PendingUpdateInfo pending_update_info_;
  base::WeakPtrFactory<ApplyPendingManifestUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_PENDING_MANIFEST_UPDATE_COMMAND_H_
