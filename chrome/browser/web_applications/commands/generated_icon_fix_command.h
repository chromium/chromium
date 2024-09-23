// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_GENERATED_ICON_FIX_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_GENERATED_ICON_FIX_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebAppIconDownloader;

// Used by metrics.
enum class GeneratedIconFixResult {
  kAppUninstalled = 0,
  kShutdown = 1,
  kDownloadFailure = 2,
  kStillGenerated = 3,
  kWriteFailure = 4,
  kSuccess = 5,

  kMaxValue = kSuccess,
};

class GeneratedIconFixCommand
    : public WebAppCommand<SharedWebContentsWithAppLock,
                           GeneratedIconFixResult> {
 public:
  explicit GeneratedIconFixCommand(
      webapps::AppId app_id,
      GeneratedIconFixSource source,
      base::OnceCallback<void(GeneratedIconFixResult)> callback);
  ~GeneratedIconFixCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(
      std::unique_ptr<SharedWebContentsWithAppLock> lock) override;

 private:
  void OnIconsDownloaded(IconsDownloadedResult result,
                         IconsMap icons_map,
                         DownloadedIconsHttpResults icons_http_results);
  void OnIconsWritten(bool success);
  void Stop(GeneratedIconFixResult result, base::Location location);

  webapps::AppId app_id_;
  GeneratedIconFixSource source_;
  std::unique_ptr<SharedWebContentsWithAppLock> lock_;

  std::unique_ptr<WebAppIconDownloader> icon_downloader_;
  std::unique_ptr<WebAppInstallInfo> install_info_;

  base::Location stop_location_;

  base::WeakPtrFactory<GeneratedIconFixCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_GENERATED_ICON_FIX_COMMAND_H_
