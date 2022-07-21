// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

InstallIsolatedAppCommand::InstallIsolatedAppCommand(
    base::StringPiece url,
    WebAppUrlLoader& url_loader,
    base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback)
    : WebAppCommand(WebAppCommandLock::CreateForAppAndWebContentsLock(
          base::flat_set<AppId>{"some random app id"})),
      url_(url),
      url_loader_(url_loader),
      data_retriever_(std::make_unique<WebAppDataRetriever>()),
      callback_(std::move(callback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  weak_this_ = weak_factory_.GetWeakPtr();
}

void InstallIsolatedAppCommand::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

InstallIsolatedAppCommand::~InstallIsolatedAppCommand() {
  DCHECK(callback_.is_null());
}

namespace {

bool IsUrlLoadingResultSuccess(WebAppUrlLoader::Result result) {
  return result == WebAppUrlLoader::Result::kUrlLoaded;
}

}  // namespace

void InstallIsolatedAppCommand::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto url = GURL{url_};
  if (!url.is_valid()) {
    ReportFailure();
    return;
  }

  url_loader_.LoadUrl(
      url, shared_web_contents(),
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&InstallIsolatedAppCommand::OnLoadUrl, weak_this_));
}

void InstallIsolatedAppCommand::OnLoadUrl(WebAppUrlLoaderResult result) {
  if (!IsUrlLoadingResultSuccess(result)) {
    ReportFailure();
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      shared_web_contents(),
      /*bypass_service_worker_check=*/false,
      base::BindOnce(
          &InstallIsolatedAppCommand::OnCheckInstallabilityAndRetrieveManifest,
          weak_this_));
}

void InstallIsolatedAppCommand::OnCheckInstallabilityAndRetrieveManifest(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_installable) {
    ReportFailure();
    return;
  }

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(valid_manifest_for_web_app)
      << "must be true when |is_installable| is true.";

  if (!opt_manifest) {
    ReportFailure();
    return;
  }

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(!blink::IsEmptyManifest(opt_manifest))
      << "must not be empty when manifest is present.";

  // See |WebAppDataRetriever::CheckInstallabilityCallback| documentation for
  // details.
  DCHECK(!manifest_url.is_empty())
      << "must not be empty if manifest is not empty.";

  Report(/*success=*/true);
}

void InstallIsolatedAppCommand::OnSyncSourceRemoved() {
  ReportFailure();
}

void InstallIsolatedAppCommand::OnShutdown() {
  ReportFailure();
}

void InstallIsolatedAppCommand::ReportFailure() {
  Report(/*success=*/false);
}

void InstallIsolatedAppCommand::Report(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  SignalCompletionAndSelfDestruct(
      success ? CommandResult::kSuccess : CommandResult::kFailure,
      base::BindOnce(std::move(callback_),
                     success ? InstallIsolatedAppCommandResult::kOk
                             : InstallIsolatedAppCommandResult::kUnknownError));
}

base::Value InstallIsolatedAppCommand::ToDebugValue() const {
  return base::Value{};
}
}  // namespace web_app
