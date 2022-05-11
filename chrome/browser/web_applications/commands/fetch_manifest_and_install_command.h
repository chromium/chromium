// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppInstallFinalizer;
class WebAppRegistrar;

// Install web app from manifest for current `WebContents`.
class FetchManifestAndInstallCommand : public WebAppCommand {
 public:
  FetchManifestAndInstallCommand(WebAppInstallFinalizer* install_finalizer,
                                 WebAppRegistrar* registrar,
                                 webapps::WebappInstallSource install_surface,
                                 base::WeakPtr<content::WebContents> contents,
                                 bool bypass_service_worker_check,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback);

  // `use_fallback` allows getting fallback information from current document
  // to enable installing a non-promotable site.
  FetchManifestAndInstallCommand(WebAppInstallFinalizer* install_finalizer,
                                 WebAppRegistrar* registrar,
                                 webapps::WebappInstallSource install_surface,
                                 base::WeakPtr<content::WebContents> contents,
                                 bool bypass_service_worker_check,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback,
                                 bool use_fallback,
                                 WebAppInstallFlow flow);

  ~FetchManifestAndInstallCommand() override;

  void Start() override;
  void OnBeforeForcedUninstallFromSync() override;
  void OnShutdown() override;

  content::WebContents* GetInstallingWebContents() override;

  base::Value ToDebugValue() const override;

 private:
  void Abort(webapps::InstallResultCode code);
  bool IsWebContentsDestroyed();

  void FetchFallbackInstallInfo();
  void OnGetWebAppInstallInfo(
      std::unique_ptr<WebAppInstallInfo> fallback_web_app_info);
  void FetchManifest();
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    bool is_installable);
  void LogInstallInfo();

  raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  raw_ptr<WebAppRegistrar> registrar_;
  webapps::WebappInstallSource install_surface_;

  base::WeakPtr<content::WebContents> web_contents_;
  bool bypass_service_worker_check_;
  WebAppInstallDialogCallback dialog_callback_;
  OnceInstallCallback install_callback_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> install_info_;

  base::Value::Dict debug_log_;

  // Whether using fallback installation data from the document.
  bool use_fallback_ = false;
  WebAppInstallFlow flow_{};

  AppId app_id_{};

  base::WeakPtrFactory<FetchManifestAndInstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_
