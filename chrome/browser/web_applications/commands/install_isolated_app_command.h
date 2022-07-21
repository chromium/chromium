// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;

namespace web_app {
class WebAppUrlLoader;
class WebAppDataRetriever;

enum class WebAppUrlLoaderResult;

enum class InstallIsolatedAppCommandResult {
  kOk,
  kUnknownError,
};

class InstallIsolatedAppCommand : public WebAppCommand {
 public:
  explicit InstallIsolatedAppCommand(
      base::StringPiece application_url,
      WebAppUrlLoader& url_loader,
      base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback);
  ~InstallIsolatedAppCommand() override;

  base::Value ToDebugValue() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void ReportFailure();
  void Report(bool success);

  void OnLoadUrl(WebAppUrlLoaderResult result);
  void OnCheckInstallabilityAndRetrieveManifest(
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      bool is_installable);

  SEQUENCE_CHECKER(sequence_checker_);

  std::string url_;

  WebAppUrlLoader& url_loader_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback_;

  base::WeakPtr<InstallIsolatedAppCommand> weak_this_;
  base::WeakPtrFactory<InstallIsolatedAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
