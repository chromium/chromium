// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_PENDING_MANIFEST_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_PENDING_MANIFEST_UPDATE_COMMAND_H_

#include <memory>

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scheduler/apply_pending_manifest_update_result.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "url/gurl.h"

namespace web_app {

// Not actually used in production logic. This is just for debugging output.
enum class ApplyPendingManifestUpdateCommandStage {
  kNotStarted,
  kAquiringAppLock,
  kSynchronizingOS,
  kDeletingPendingIconDirectories,
  kDeletingPendingUpdateInfo,
};

class ApplyPendingManifestUpdateCommand
    : public WebAppCommand<AppLock, ApplyPendingManifestUpdateResult> {
 public:
  using PassKey = base::PassKey<ApplyPendingManifestUpdateCommand>;
  ApplyPendingManifestUpdateCommand(
      const webapps::AppId& app_id,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ApplyPendingManifestUpdateCompletedCallback callback);

  ~ApplyPendingManifestUpdateCommand() override;

 protected:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  // Sets the pending update info to the web app.
  void ApplyPendingUpdateInfoToWebApp(bool success);

  // Deletes pending trusted icons and pending manifest icons directory from the
  // disk.
  void DeletePendingIconsFromDisk();

  // Sets the pending update info as a nullopt.
  void DeletePendingUpdateInfoThenComplete(
      ApplyPendingManifestUpdateResult expected_result);
  void CompleteCommandAndSelfDestruct(
      ApplyPendingManifestUpdateResult check_result);

  void SetStage(ApplyPendingManifestUpdateCommandStage stage);

  base::WeakPtr<ApplyPendingManifestUpdateCommand> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  std::unique_ptr<AppLock> lock_;
  const webapps::AppId app_id_;

  // Debug info.
  ApplyPendingManifestUpdateCommandStage stage_ =
      ApplyPendingManifestUpdateCommandStage::kNotStarted;

  // KeepAlive objects are needed to make sure that manifest update writes
  // still happen even though the app window has closed.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  bool has_icon_changes_ = false;
  bool has_name_change_ = false;
  base::WeakPtrFactory<ApplyPendingManifestUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_PENDING_MANIFEST_UPDATE_COMMAND_H_
