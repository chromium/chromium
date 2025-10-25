
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_URL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_URL_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
enum class InstallableStatusCode;
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

enum class IconsDownloadedResult;
class SharedWebContentsWithAppLock;
class WebAppDataRetriever;

using WebInstallFromUrlCommandCallback =
    base::OnceCallback<void(const webapps::AppId& app_id,
                            webapps::InstallResultCode code)>;

using IconUrlSizeSet = base::flat_set<IconUrlWithSize>;

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// A map of |IconUrlWithSize| to http status results. `http_status_code` is
// never 0.
using DownloadedIconsHttpResults =
    base::flat_map<IconUrlWithSize, int /*http_status_code*/>;

// Implementation of the Web Install API for loading an install_url.
// Fetches the content at `install_url` in the background document.
//
// If no `manifest_id` is provided, the fetched content's manifest file must
// contain a developer-specified id field (computed is not sufficient).
// Otherwise, the provided `manifest_id` must match the fetched manifest's
// computed id.
//
// If the id validation succeeds, then the install dialog is shown and the app
// is installed if accepted. After installation, the app is launched. This
// command will return errors for state such as the url not loading, the site
// not being "installable", the `manifest_id` doesn't exist or doesn't match,
// or if the user rejects the installation.
class WebInstallFromUrlCommand
    : public WebAppCommand<SharedWebContentsLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode> {
 public:
  WebInstallFromUrlCommand(Profile& profile,
                           const GURL& install_url,
                           const std::optional<GURL>& manifest_id,
                           base::WeakPtr<content::WebContents> web_contents,
                           const GURL& last_committed_url,
                           WebAppInstallDialogCallback dialog_callback,
                           WebInstallFromUrlCommandCallback installed_callback);
  ~WebInstallFromUrlCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  content::WebContents* shared_web_contents();
  void Abort(webapps::InstallResultCode code);
  bool IsWebContentsDestroyed();
  void OnUrlLoadedFetchManifest(webapps::WebAppUrlLoaderResult result);
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    bool valid_manifest_for_web_app,
                                    webapps::InstallableStatusCode error_code);
  void CreateWebAppInstallInfoFromManifest();
  void OnWebAppInstallInfoCreatedShowDialog(
      std::unique_ptr<WebAppInstallInfo> install_info);
  void OnInstallDialogCompleted(
      bool user_accepted,
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void InstallApp();
  void OnAppInstalled(const webapps::AppId& app_id,
                      webapps::InstallResultCode code);
  void LaunchApp();
  void OnAppLaunched(base::Value launch_debug_value);
  void MeasureUserInstalledAppHistogram(webapps::InstallResultCode code);

  raw_ref<Profile> profile_;
  // Unset if the WebInstall API's 1-parameter signature was called.
  std::optional<GURL> manifest_id_;
  GURL install_url_;
  // The WebContents that initiated the install. This is used only to show the
  // install dialog.
  base::WeakPtr<content::WebContents> web_contents_;
  // The last committed URL of the page that initiated the install.
  GURL last_committed_url_;
  WebAppInstallDialogCallback dialog_callback_;
  InstallErrorLogEntry install_error_log_entry_;

  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;
  std::unique_ptr<SharedWebContentsWithAppLock>
      shared_web_contents_with_app_lock_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  IconUrlSizeSet icons_from_manifest_;
  webapps::InstallResultCode install_result_code_;
  blink::mojom::ManifestPtr opt_manifest_;
  webapps::AppId app_id_;

  base::WeakPtrFactory<WebInstallFromUrlCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_URL_COMMAND_H_
