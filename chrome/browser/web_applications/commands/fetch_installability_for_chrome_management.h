// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class NoopLock;
class NoopLockDescription;
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
                            absl::optional<webapps::AppId> app_id)>;

// Given a url and web contents, this command determines if the given url is
// installable, what the webapps::AppId is, and if it is already installed.
class FetchInstallabilityForChromeManagement
    : public WebAppCommandTemplate<NoopLock> {
 public:
  FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      FetchInstallabilityForChromeManagementCallback callback);
  ~FetchInstallabilityForChromeManagement() override;

  // WebAppCommandTemplate<NoopLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<NoopLock>) override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  void OnUrlLoadedCheckInstallability(WebAppUrlLoaderResult result);
  void OnWebAppInstallabilityChecked(blink::mojom::ManifestPtr opt_manifest,
                                     const GURL& manifest_url,
                                     bool valid_manifest_for_web_app,
                                     webapps::InstallableStatusCode error_code);
  void OnAppLockGranted(std::unique_ptr<AppLock> app_lock);

  void Abort(InstallableCheckResult result);
  bool IsWebContentsDestroyed();

  std::unique_ptr<NoopLockDescription> noop_lock_description_;
  std::unique_ptr<AppLockDescription> app_lock_description_;

  std::unique_ptr<AppLock> app_lock_;
  std::unique_ptr<NoopLock> noop_lock_;

  const GURL url_;
  webapps::AppId app_id_;

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
