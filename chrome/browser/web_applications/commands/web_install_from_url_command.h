
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_URL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_URL_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
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
    base::OnceCallback<void(const GURL& manifest_id,
                            webapps::InstallResultCode code)>;

using IconUrlSizeSet = base::flat_set<IconUrlWithSize>;

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// A map of |IconUrlWithSize| to http status results. `http_status_code` is
// never 0.
using DownloadedIconsHttpResults =
    base::flat_map<IconUrlWithSize, int /*http_status_code*/>;

// Implementation of the Web Install API for loading an install_url.
// Fetches the content at `install_url` in the background document. If the
// provided `manifest_id` matches the computed manifest id, then the install
// dialog is shown and the app is installed if accepted. After installation, the
// app is launched.
// This command will return errors for state such as the url not loading, the
// site not being "installable", the `manifest_id` doesn't match, or if the user
// rejects the installation.
class WebInstallFromUrlCommand
    : public WebAppCommand<SharedWebContentsLock,
                           const GURL&,
                           webapps::InstallResultCode> {
 public:
  WebInstallFromUrlCommand(Profile& profile,
                           const GURL& manifest_id,
                           const GURL& install_url,
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
  void GetIcons();
  void OnIconsRetrievedShowDialog(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnInstallDialogCompleted(bool user_accepted);
  void InstallApp();
  void OnAppInstalled(const webapps::AppId& app_id,
                      webapps::InstallResultCode code);
  void LaunchApp();
  void OnAppLaunched(base::Value launch_debug_value);
  void MeasureUserInstalledAppHistogram(webapps::InstallResultCode code);

  raw_ref<Profile> profile_;
  GURL manifest_id_;
  GURL install_url_;
  InstallErrorLogEntry install_error_log_entry_;

  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;
  std::unique_ptr<SharedWebContentsWithAppLock>
      shared_web_contents_with_app_lock_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  IconUrlSizeSet icons_from_manifest_;
  webapps::InstallResultCode install_result_code_;
  blink::mojom::ManifestPtr opt_manifest_;
  webapps::AppId app_id_;

  base::WeakPtrFactory<WebInstallFromUrlCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_URL_COMMAND_H_
