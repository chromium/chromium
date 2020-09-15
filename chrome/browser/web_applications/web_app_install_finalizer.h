// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace web_app {

class WebApp;
class WebAppIconManager;
class WebAppRegistrar;

class WebAppInstallFinalizer final : public InstallFinalizer {
 public:
  // |legacy_finalizer| can be nullptr (optional argument).
  WebAppInstallFinalizer(Profile* profile,
                         WebAppIconManager* icon_manager,
                         std::unique_ptr<InstallFinalizer> legacy_finalizer);
  ~WebAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void FinalizeUninstallAfterSync(const AppId& app_id,
                                  UninstallWebAppCallback callback) override;
  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      InstallFinalizedCallback callback) override;
  void UninstallExternalWebApp(const AppId& app_id,
                               ExternalInstallSource external_install_source,
                               UninstallWebAppCallback callback) override;
  bool CanUserUninstallFromSync(const AppId& app_id) const override;
  void UninstallWebAppFromSyncByUser(const AppId& app_id,
                                     UninstallWebAppCallback callback) override;
  bool CanUserUninstallExternalApp(const AppId& app_id) const override;
  void UninstallExternalAppByUser(const AppId& app_id,
                                  UninstallWebAppCallback callback) override;
  bool WasExternalAppUninstalledByUser(const AppId& app_id) const override;
  void RemoveLegacyInstallFinalizerForTesting() override;
  InstallFinalizer* legacy_finalizer_for_testing() override;
  void Start() override;
  void Shutdown() override;

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;

  void UninstallWebApp(const AppId& app_id, UninstallWebAppCallback callback);
  void UninstallWebAppOrRemoveSource(const AppId& app_id,
                                     Source::Type source,
                                     UninstallWebAppCallback callback);

  void SetWebAppManifestFieldsAndWriteData(
      const WebApplicationInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback);

  void OnIconsDataWritten(
      CommitCallback commit_callback,
      std::unique_ptr<WebApp> web_app,
      const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps,
      bool success);

  void OnShortcutsMenuIconsDataWritten(CommitCallback commit_callback,
                                       std::unique_ptr<WebApp> web_app,
                                       bool success);

  void OnIconsDataDeleted(const AppId& app_id,
                          UninstallWebAppCallback callback,
                          bool success);
  void OnDatabaseCommitCompletedForInstall(InstallFinalizedCallback callback,
                                           AppId app_id,
                                           bool success);
  void OnDatabaseCommitCompletedForUpdate(
      InstallFinalizedCallback callback,
      AppId app_id,
      std::string old_name,
      const WebApplicationInfo& web_app_info,
      bool success);

  WebAppRegistrar& GetWebAppRegistrar() const;

  std::unique_ptr<InstallFinalizer> legacy_finalizer_;

  Profile* const profile_;
  WebAppIconManager* const icon_manager_;
  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallFinalizer);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
