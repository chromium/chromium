// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace content {
class WebContents;
}

namespace web_app {

class NoopLock;
class AppLock;
class WebAppDataRetriever;
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;

enum class InstallableCheckResult {
  kNotInstallable,
  kInstallable,
  kAlreadyInstalled,
};
using FetchInstallabilityForChromeManagementCallback =
    base::OnceCallback<void(InstallableCheckResult result,
                            absl::optional<AppId> app_id)>;

// Given a url and web contents, this command determines if the given url is
// installable, what the AppId is, and if it is already installed.
class FetchInstallabilityForChromeManagement : public WebAppCommand {
 public:
  FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      const WebAppRegistrar& registry,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      FetchInstallabilityForChromeManagementCallback callback);
  ~FetchInstallabilityForChromeManagement() override;

  Lock& lock() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  base::Value ToDebugValue() const override;

 private:
  void OnUrlLoadedCheckInstallability(WebAppUrlLoaderResult result);
  void OnWebAppInstallabilityChecked(blink::mojom::ManifestPtr opt_manifest,
                                     const GURL& manifest_url,
                                     bool valid_manifest_for_web_app,
                                     bool is_installable);
  void OnAppLockGranted();

  void Abort(InstallableCheckResult result);
  bool IsWebContentsDestroyed();

  std::unique_ptr<NoopLock> noop_lock_;
  std::unique_ptr<AppLock> app_lock_;
  const GURL url_;
  AppId app_id_;
  // The registry is owned by the WebAppProvider, and is always destroyed after
  // the CommandManager, so this is safe.
  const base::raw_ref<const WebAppRegistrar> registry_;
  base::WeakPtr<content::WebContents> web_contents_;
  const std::unique_ptr<WebAppUrlLoader> url_loader_;
  const std::unique_ptr<WebAppDataRetriever> data_retriever_;
  FetchInstallabilityForChromeManagementCallback callback_;

  base::Value::List error_log_;

  base::WeakPtrFactory<FetchInstallabilityForChromeManagement> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_H_
