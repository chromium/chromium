// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APP_UPDATE_DATA_READ_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APP_UPDATE_DATA_READ_COMMAND_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

// The result of constructing the required metadata for an app update dialog to
// be shown to the user for `app_id`. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.

// LINT.IfChange(AppUpdateDataReadResult)

enum class AppUpdateDataReadResult {
  // kFlagNotEnabled = 0, (Deprecated, do not use).
  // App is not installed.
  kAppNotInstalled = 1,
  // App does not have any pending update metadata.
  kAppDoesNotHavePendingUpdate = 2,
  // Failed to read existing app icons for update dialog.
  kFailedToReadExistingAppIcons = 3,
  // Failed to read pending app icons for update dialog.
  kFailedToReadPendingAppIconsWhenRequested = 4,
  // System shutdown in the middle of running.
  kSystemShutdown = 5,
  // Data parsing successful, the user can see the dialog.
  kSuccess = 6,
  // App has insignificant change after setting update fields.
  kInsignificantChangeAfterUpdate = 7,
  kMaxValue = kInsignificantChangeAfterUpdate
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppUpdateDataReadResult)

using UpdateMetadata = std::optional<WebAppIdentityUpdate>;

// Parse a web app's pending update metadata and icons stored on the disk to
// construct a `WebAppIdentityUpdate` instance that can be use to show the app
// identity update dialog.
class AppUpdateDataReadCommand
    : public WebAppCommand<AppLock, AppUpdateDataReadResult, UpdateMetadata> {
 public:
  AppUpdateDataReadCommand(
      const webapps::AppId& app_id,
      Profile* profile,
      base::OnceCallback<void(UpdateMetadata)> completed_callback);
  ~AppUpdateDataReadCommand() override;

 protected:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void OnIconFetchedMaybeMaskForUpdate(IconMetadataForUpdate icon_metadata);

  // If concurrent closures are used for masking, then a race condition is
  // created where the from bitmap might get processed before the to bitmap, and
  // setting them inside `update_` becomes complicated. These functions are a
  // way of fixing that behavior, by explicitly setting the correct icon.
  void SetOldIconForIdentityUpdate(SkBitmap old_icon);
  void SetNewIconForIdentityUpdate(SkBitmap new_icon);
  void OnIconsProcessedCreateIdentity();

  // Marks the icon diffs as insignificant after comparing them by reading from
  // the disk.
  //
  // This can happen for apps that have no trusted icons on the disk, and the
  // icon reading code falls back to reading manifest icons. Since the trusted
  // icon is a subset of the manifest icons, the icons that show up on the
  // dialog look the same.
  //
  // For this case, the update will be applied silently.
  void OnImageDiffComputedUpdateIdentity(bool more_than_ten_percent_diff);
  void ReportResultAndDestroy(AppUpdateDataReadResult data_read_result);

  std::unique_ptr<AppLock> lock_;
  const webapps::AppId app_id_;
  proto::PendingUpdateInfo pending_update_info_;
  WebAppIdentityUpdate update_;
  const raw_ref<Profile> profile_;

  base::WeakPtrFactory<AppUpdateDataReadCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APP_UPDATE_DATA_READ_COMMAND_H_
