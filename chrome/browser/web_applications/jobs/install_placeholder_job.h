// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_INSTALL_PLACEHOLDER_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_INSTALL_PLACEHOLDER_JOB_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"

class Profile;

namespace content {
class WebContents;
}

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

class SharedWebContentsWithAppLock;
class WebAppDataRetriever;

// This job is used during externally managed app install flow to install a
// placeholder app instead of the target app when the app's install_url fails to
// load.
class InstallPlaceholderJob {
 public:
  using InstallAndReplaceCallback =
      base::OnceCallback<void(webapps::InstallResultCode code,
                              webapps::AppId app_id)>;
  InstallPlaceholderJob(Profile* profile,
                        base::Value::Dict& debug_value,
                        const ExternalInstallOptions& install_options,
                        InstallAndReplaceCallback callback,
                        SharedWebContentsWithAppLock& lock);
  virtual ~InstallPlaceholderJob();

  void Start();

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void Abort(webapps::InstallResultCode code);
  void FetchCustomIcon(const GURL& url, int retries_left);

  void OnUrlLoaded(webapps::WebAppUrlLoaderResult load_url_result);
  void OnCustomIconFetched(const GURL& image_url,
                           int retries_left,
                           IconsDownloadedResult result,
                           IconsMap icons_map,
                           DownloadedIconsHttpResults icons_http_results);

  void FinalizeInstall(
      std::optional<std::reference_wrapper<const std::vector<SkBitmap>>>
          bitmaps);

  void OnInstallFinalized(const webapps::AppId& app_id,
                          webapps::InstallResultCode code);

  const raw_ref<Profile> profile_;
  const raw_ref<base::Value::Dict> debug_value_;
  const webapps::AppId app_id_;

  // `this` must exist within the scope of a WebCommand's
  // SharedWebContentsWithAppLock.
  const raw_ref<SharedWebContentsWithAppLock> lock_;

  const ExternalInstallOptions install_options_;
  InstallAndReplaceCallback callback_;

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::WeakPtrFactory<InstallPlaceholderJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_INSTALL_PLACEHOLDER_JOB_H_
