// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class WebAppDataRetriever;
class WebAppInstallFinalizer;
class WebAppUrlLoader;

enum class WebAppUrlLoaderResult;

struct InstallIsolatedAppCommandSuccess {};
struct InstallIsolatedAppCommandError {
  std::string message;

  friend std::ostream& operator<<(std::ostream& os,
                                  const InstallIsolatedAppCommandError& error) {
    return os << "InstallIsolatedAppCommandError { message = \""
              << error.message << "\" }.";
  }
};

// Isolated Web App requires:
//  * no cross-origin navigation
//  * content should never be loaded in normal tab
//
// |content::IsolatedAppThrottle| enforces that. The requirements prevent
// re-using web contents.
class InstallIsolatedAppCommand : public WebAppCommand {
 public:
  // |application_url| is the url for the app to be installed. The url must be
  // valid.
  //
  // |callback| must be not null.
  //
  // The `id` in the application's manifest must equal "/".
  explicit InstallIsolatedAppCommand(
      const GURL& application_url,
      const IsolationData& isolation_data,
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      WebAppInstallFinalizer& install_finalizer,
      base::OnceCallback<void(base::expected<InstallIsolatedAppCommandSuccess,
                                             InstallIsolatedAppCommandError>)>
          callback);

  InstallIsolatedAppCommand(const InstallIsolatedAppCommand&) = delete;
  InstallIsolatedAppCommand& operator=(const InstallIsolatedAppCommand&) =
      delete;

  InstallIsolatedAppCommand(InstallIsolatedAppCommand&&) = delete;
  InstallIsolatedAppCommand& operator=(InstallIsolatedAppCommand&&) = delete;

  ~InstallIsolatedAppCommand() override;

  Lock& lock() const override;

  base::Value ToDebugValue() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void ReportFailure(base::StringPiece message);
  void ReportSuccess();

  void DownloadIcons(WebAppInstallInfo install_info);
  void OnGetIcons(WebAppInstallInfo install_info,
                  IconsDownloadedResult result,
                  std::map<GURL, std::vector<SkBitmap>> icons_map,
                  std::map<GURL, int /*http_status_code*/> icons_http_results);

  void LoadUrl();
  void OnLoadUrl(WebAppUrlLoaderResult result);

  void CheckInstallabilityAndRetrieveManifest();
  void OnCheckInstallabilityAndRetrieveManifest(
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      bool is_installable);
  base::expected<WebAppInstallInfo, std::string> CreateInstallInfoFromManifest(
      const blink::mojom::Manifest& manifest,
      const GURL& manifest_url);
  void FinalizeInstall(const WebAppInstallInfo& info);
  void OnFinalizeInstall(const AppId& unused_app_id,
                         webapps::InstallResultCode install_result_code,
                         OsHooksErrors unused_os_hooks_errors);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<AppLock> lock_;

  GURL url_;
  IsolationData isolation_data_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<WebAppUrlLoader> url_loader_;

  WebAppInstallFinalizer& install_finalizer_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::OnceCallback<void(base::expected<InstallIsolatedAppCommandSuccess,
                                         InstallIsolatedAppCommandError>)>
      callback_;

  base::WeakPtrFactory<InstallIsolatedAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
