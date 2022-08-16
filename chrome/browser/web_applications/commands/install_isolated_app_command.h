// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;

namespace web_app {

class SharedWebContentsWithAppLock;
class WebAppDataRetriever;
class WebAppInstallFinalizer;
class WebAppUrlLoader;

enum class WebAppUrlLoaderResult;

enum class InstallIsolatedAppCommandResult {
  kOk,
  kUnknownError,
};

class InstallIsolatedAppCommand : public WebAppCommand {
 public:
  // TODO(kuragin): Consider to create an instance of |GURL| instead of passing
  // a string and probably introduce factory function in order to handle invalid
  // urls.
  //
  // |application_url| is the url for the app to be installed.
  //
  // |callback| must be not null.
  //
  // The `id` in the application's manifest must equal "/".
  explicit InstallIsolatedAppCommand(
      base::StringPiece application_url,
      WebAppUrlLoader& url_loader,
      WebAppInstallFinalizer& install_finalizer,
      base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback);
  ~InstallIsolatedAppCommand() override;

  Lock& lock() const override;

  base::Value ToDebugValue() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void ReportFailure();
  void ReportSuccess();
  void Report(bool success);

  void DownloadIcons();

  void LoadUrl(GURL url);
  void OnLoadUrl(WebAppUrlLoaderResult result);

  void CheckInstallabilityAndRetrieveManifest();
  void OnCheckInstallabilityAndRetrieveManifest(
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      bool is_installable);
  absl::optional<WebAppInstallInfo> CreateInstallInfoFromManifest(
      const blink::mojom::Manifest& manifest,
      const GURL& manifest_url);
  void FinalizeInstall(const WebAppInstallInfo& info);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<SharedWebContentsWithAppLock> lock_;

  std::string url_;

  WebAppUrlLoader& url_loader_;
  WebAppInstallFinalizer& install_finalizer_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback_;

  base::WeakPtrFactory<InstallIsolatedAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
