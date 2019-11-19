// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class Profile;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

class AppShortcutManager;
class InstallFinalizer;
class WebAppDataRetriever;
class WebAppUrlLoader;

class WebAppInstallTask : content::WebContentsObserver {
 public:
  using RetrieveWebApplicationInfoWithIconsCallback =
      base::OnceCallback<void(std::unique_ptr<WebApplicationInfo>)>;

  WebAppInstallTask(Profile* profile,
                    AppShortcutManager* shortcut_manager,
                    InstallFinalizer* install_finalizer,
                    std::unique_ptr<WebAppDataRetriever> data_retriever);
  ~WebAppInstallTask() override;

  // Request the app_id expectation check. Install fails with
  // kExpectedAppIdCheckFailed if actual app_id doesn't match expected app_id.
  // The actual resulting app_id is reported as a part of OnceInstallCallback.
  void ExpectAppId(const AppId& expected_app_id);

  using LoadWebAppAndCheckInstallabilityCallback = base::OnceCallback<void(
      std::unique_ptr<content::WebContents> web_contents,
      const AppId& app_id,
      InstallResultCode code)>;
  // Load a web app from the given URL and check installability.
  void LoadWebAppAndCheckInstallability(
      const GURL& url,
      WebappInstallSource install_source,
      WebAppUrlLoader* url_loader,
      LoadWebAppAndCheckInstallabilityCallback callback);

  // Checks a WebApp installability, retrieves manifest and icons and
  // then performs the actual installation.
  void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // This method infers WebApp info from the blink renderer process
  // and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifest|. If manifest is incomplete or missing, the
  // inferred info is used.
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // Load |launch_url| and do silent background install with
  // |InstallWebAppFromManifestWithFallback|. Posts |LoadUrl| task to
  // |url_loader| immediately. Doesn't memorize |url_loader| pointer.
  void LoadAndInstallWebAppFromManifestWithFallback(
      const GURL& launch_url,
      content::WebContents* web_contents,
      WebAppUrlLoader* url_loader,
      WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // InstallManager doesn't fetch a manifest.
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // Starts a background web app installation process for a given
  // |web_contents|. This method infers WebApp info from the blink renderer
  // process and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifestWithFallback|.
  void InstallWebAppWithParams(
      content::WebContents* web_contents,
      const InstallManager::InstallParams& install_params,
      WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // Starts background installation of a web app: does not show UI dialog.
  // |web_application_info| contains all the data needed for installation. Icons
  // will be downloaded from the icon URLs provided in |web_application_info|.
  void InstallWebAppFromInfoRetrieveIcons(
      content::WebContents* web_contents,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool is_locally_installed,
      WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  void UpdateWebAppFromInfo(
      content::WebContents* web_contents,
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      InstallManager::OnceInstallCallback callback);

  // Obtains WebApplicationInfo about web app located at |app_url|, fallbacks to
  // title/favicon if manifest is not present.
  void LoadAndRetrieveWebApplicationInfoWithIcons(
      const GURL& app_url,
      WebAppUrlLoader* url_loader,
      RetrieveWebApplicationInfoWithIconsCallback callback);

  static std::unique_ptr<content::WebContents> CreateWebContents(
      Profile* profile);

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  void SetInstallFinalizerForTesting(InstallFinalizer* install_finalizer);

 private:
  void CheckInstallPreconditions();
  void RecordInstallEvent(ForInstallableSite for_installable_site);

  // Calling the callback may destroy |this| task. Callers shouldn't work with
  // any |this| class members after calling it.
  void CallInstallCallback(const AppId& app_id, InstallResultCode code);

  // Checks if any errors occurred while |this| was async awaiting. All On*
  // completion handlers below must return early if this is true. Also, if
  // ShouldStopInstall is true, install_callback_ is already invoked or may be
  // invoked later: All On* completion handlers don't need to call
  // install_callback_.
  bool ShouldStopInstall() const;

  void OnWebAppUrlLoadedGetWebApplicationInfo(WebAppUrlLoader::Result result);

  void OnWebAppUrlLoadedCheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      WebAppUrlLoader::Result result);
  void OnWebAppInstallabilityChecked(
      base::Optional<blink::Manifest> opt_manifest,
      bool valid_manifest_for_web_app,
      bool is_installable);

  void OnGetWebApplicationInfo(
      bool force_shortcut_app,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnDidPerformInstallableCheck(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool force_shortcut_app,
      base::Optional<blink::Manifest> opt_manifest,
      bool valid_manifest_for_web_app,
      bool is_installable);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and
  void CheckForPlayStoreIntentOrGetIcons(
      const blink::Manifest& manifest,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      std::vector<GURL> icon_urls,
      ForInstallableSite for_installable_site,
      bool skip_page_favicons);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      std::vector<GURL> icon_urls,
      ForInstallableSite for_installable_site,
      bool skip_page_favicons,
      const std::string& intent,
      bool should_intent_to_store);

  void OnIconsRetrieved(std::unique_ptr<WebApplicationInfo> web_app_info,
                        bool is_locally_installed,
                        IconsMap icons_map);
  void OnIconsRetrievedShowDialog(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      IconsMap icons_map);
  void OnIconsRetrievedFinalizeUpdate(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      IconsMap icons_map);
  void OnDialogCompleted(ForInstallableSite for_installable_site,
                         bool user_accepted,
                         std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnInstallFinalized(const AppId& app_id, InstallResultCode code);
  void OnInstallFinalizedCreateShortcuts(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      const AppId& app_id,
      InstallResultCode code);
  void OnShortcutsCreated(std::unique_ptr<WebApplicationInfo> web_app_info,
                          const AppId& app_id,
                          bool shortcut_created);

  // Whether we should just obtain WebApplicationInfo instead of the actual
  // installation.
  bool only_retrieve_web_application_info_ = false;

  InstallManager::WebAppInstallDialogCallback dialog_callback_;
  InstallManager::OnceInstallCallback install_callback_;
  RetrieveWebApplicationInfoWithIconsCallback retrieve_info_callback_;
  base::Optional<InstallManager::InstallParams> install_params_;
  base::Optional<AppId> expected_app_id_;
  bool background_installation_ = false;

  // The mechanism via which the app creation was triggered, will stay as
  // kNoInstallSource for updates.
  static constexpr WebappInstallSource kNoInstallSource =
      WebappInstallSource::COUNT;
  WebappInstallSource install_source_ = kNoInstallSource;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebApplicationInfo> web_application_info_;
  std::unique_ptr<content::WebContents> web_contents_;

  AppShortcutManager* shortcut_manager_;
  InstallFinalizer* install_finalizer_;
  Profile* const profile_;

  base::WeakPtrFactory<WebAppInstallTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallTask);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
