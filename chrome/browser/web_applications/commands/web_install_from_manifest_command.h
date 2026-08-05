// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_MANIFEST_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_MANIFEST_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace web_app {

class FinalizeInstallOrUpdateJob;
class ManifestToWebAppInstallInfoJob;
class SharedWebContentsWithAppLock;
class WebAppDataRetriever;

using WebInstallFromManifestCommandCallback =
    base::OnceCallback<void(const webapps::AppId& app_id,
                            webapps::InstallResultCode code)>;

// Implementation of the Web Install API for the manifest_url flow.
// Installs a web app using a pre-parsed manifest, eliminating the traditional
// document load step.
//
// The manifest has already been fetched and parsed by the service impl
// (WebInstallManifestFetcher + ParseManifestFromManifestUrlCommand). This
// command converts the manifest to WebAppInstallInfo, downloads icons via the
// shared web contents (about:blank context — no user cookies), shows the
// install dialog on the initiating page, and finalizes the install.
class WebInstallFromManifestCommand
    : public WebAppCommand<SharedWebContentsLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode>,
      public content::WebContentsObserver {
 public:
  WebInstallFromManifestCommand(
      Profile& profile,
      blink::mojom::ManifestPtr manifest,
      const GURL& manifest_url,
      base::WeakPtr<content::WebContents> initiating_web_contents,
      base::WeakPtr<content::Page> initiating_page,
      const GURL& requesting_page_url,
      WebAppInstallDialogCallback dialog_callback,
      WebInstallFromManifestCommandCallback installed_callback);
  ~WebInstallFromManifestCommand() override;

 protected:
  // WebAppCommand:
  content::WebContents* GetInstallingWebContents(
      base::PassKey<WebAppCommandManager>) override;
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  void OnWebAppInstallInfoCreatedShowDialog(
      std::unique_ptr<WebAppInstallInfo> install_info);
  void OnAppLockAcquired();
  void OnInstallDialogCompleted(
      bool user_accepted,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      WebAppInstallationAcceptanceResultCallback result_callback);
  void OnAppInstalled(const webapps::AppId& app_id,
                      webapps::InstallResultCode code);

  void Abort(webapps::InstallResultCode code);
  // Returns true if the initiating page went away during one of this command's
  // async steps. Prevents cross-document navigation from spoofing the origin
  // shown in the dialog.
  bool IsInitiatingPageGone() const;
  void MeasureUserInstalledAppHistogram(webapps::InstallResultCode code);

  raw_ref<Profile> profile_;
  blink::mojom::ManifestPtr manifest_;
  GURL manifest_url_;
  // The WebContents that initiated the install. Used to show the install
  // dialog. Also used to detect cross-document navigations.
  base::WeakPtr<content::WebContents> initiating_web_contents_;
  // WeakPtr to the Page that was active when the install was invoked. Used to
  // detect whether the initiating page has been navigated away.
  base::WeakPtr<content::Page> initiating_page_;
  // The last committed URL of the page that initiated the install.
  GURL requesting_page_url_;
  // Set to true if the initiating page changed before the command started
  // executing (i.e. before StartWithLock acquired the lock).
  bool page_changed_before_start_ = false;
  WebAppInstallDialogCallback dialog_callback_;
  WebAppInstallationAcceptanceResultCallback acceptance_result_callback_;

  // SharedWebContentsLock is held while downloading icons.
  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;

  // SharedWebContentsWithAppLock is held while installing the app.
  std::unique_ptr<SharedWebContentsWithAppLock>
      shared_web_contents_with_app_lock_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  std::unique_ptr<FinalizeInstallOrUpdateJob> install_job_;

  base::WeakPtrFactory<WebInstallFromManifestCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_INSTALL_FROM_MANIFEST_COMMAND_H_
