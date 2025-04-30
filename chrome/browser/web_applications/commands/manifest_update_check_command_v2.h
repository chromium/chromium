// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_V2_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_V2_H_

#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

struct WebAppInstallInfo;

// Documentation: docs/webapps/manifest_update_process.md
//
// Checks whether the installed web app associated with a given WebContents has
// out of date manifest data and what to update it to.
//
// High level procedure for this command:
// - Download new manifest data from site including external resources (such as
//   icon bitmaps only if the url changes from the saved information).
// - Load existing manifest data from disk including external resources.
// - Diff manifest data.
// - Resolve any changes to app identity by confirming the change with the user,
//   silently allowing them, or reverting them.
// - Return back to the caller to schedule applying the changes back to disk.
//
// TODO(crbug.com/414851433): Rename this to ManifestUpdateCheckCommand and
// remove existing ManifestUpdateCheckCommand.
class ManifestUpdateCheckCommandV2
    : public WebAppCommand<AppLock,
                           ManifestUpdateCheckResult,
                           std::unique_ptr<WebAppInstallInfo>>,
      public content::WebContentsObserver {
 public:
  using CompletedCallback = base::OnceCallback<void(
      ManifestUpdateCheckResult check_result,
      std::unique_ptr<WebAppInstallInfo> new_install_info)>;

  ManifestUpdateCheckCommandV2(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> web_contents,
      CompletedCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<WebAppIconDownloader> icon_downloader);

  ~ManifestUpdateCheckCommandV2() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  // Stage: Update check complete.
  // (ManifestUpdateCheckStage::kComplete)
  bool IsWebContentsDestroyed();
  void CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult check_result);

  base::WeakPtr<ManifestUpdateCheckCommandV2> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Manifest update check request parameters.
  const GURL url_;
  const webapps::AppId app_id_;
  base::Time check_time_;

  // Resources and helpers used to fetch manifest data.
  std::unique_ptr<AppLock> lock_;
  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppIconDownloader> icon_downloader_;

  // Temporary variables stored here while the update check progresses
  // asynchronously.
  std::unique_ptr<WebAppInstallInfo> new_install_info_;

  // Debug info.
  ManifestUpdateCheckStage stage_ = ManifestUpdateCheckStage::kPendingAppLock;

  base::WeakPtrFactory<ManifestUpdateCheckCommandV2> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_V2_H_
