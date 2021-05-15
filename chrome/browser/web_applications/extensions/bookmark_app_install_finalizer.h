// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/constants.h"

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}

namespace extensions {

class CrxInstaller;
class Extension;

// Class used to actually install the Bookmark App in the system.
// TODO(crbug.com/1065748): Erase this subclass.
class BookmarkAppInstallFinalizer : public web_app::InstallFinalizer {
 public:
  // Constructs a BookmarkAppInstallFinalizer that will install the Bookmark App
  // in |profile|.
  explicit BookmarkAppInstallFinalizer(Profile* profile);
  BookmarkAppInstallFinalizer(const BookmarkAppInstallFinalizer&) = delete;
  BookmarkAppInstallFinalizer& operator=(const BookmarkAppInstallFinalizer&) =
      delete;
  ~BookmarkAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void FinalizeUninstallAfterSync(const web_app::AppId& app_id,
                                  UninstallWebAppCallback callback) override;
  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      content::WebContents* web_contents,
                      InstallFinalizedCallback callback) override;
  void UninstallExternalWebApp(
      const web_app::AppId& app_id,
      webapps::WebappUninstallSource webapp_uninstall_source,
      UninstallWebAppCallback callback) override;
  bool CanUserUninstallWebApp(const web_app::AppId& app_id) const override;
  void UninstallWebApp(const web_app::AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       UninstallWebAppCallback callback) override;
  bool WasPreinstalledWebAppUninstalled(
      const web_app::AppId& app_id) const override;

  using CrxInstallerFactory =
      base::RepeatingCallback<scoped_refptr<CrxInstaller>(Profile*)>;
  void SetCrxInstallerFactoryForTesting(
      CrxInstallerFactory crx_installer_factory);

 private:
  // May return nullptr if app_id is not found or extension is disabled.
  const Extension* GetEnabledExtension(const web_app::AppId& app_id) const;

  void UninstallExtension(const web_app::AppId& app_id,
                          UninstallWebAppCallback);

  void OnExtensionInstalled(const GURL& start_url,
                            LaunchType launch_type,
                            bool enable_experimental_tabbed_window,
                            bool is_locally_installed,
                            InstallFinalizedCallback callback,
                            scoped_refptr<CrxInstaller> crx_installer,
                            const absl::optional<CrxInstallError>& error);

  void OnExtensionUpdated(const web_app::AppId& expected_app_id,
                          const std::string& old_name,
                          const WebApplicationInfo& web_app_info,
                          InstallFinalizedCallback callback,
                          scoped_refptr<CrxInstaller> crx_installer,
                          const absl::optional<CrxInstallError>& error);

  CrxInstallerFactory crx_installer_factory_;
  web_app::ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  Profile* profile_;

  base::WeakPtrFactory<BookmarkAppInstallFinalizer> weak_ptr_factory_{this};

};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
