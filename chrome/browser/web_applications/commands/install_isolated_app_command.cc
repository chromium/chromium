// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

bool IsUrlLoadingResultSuccess(WebAppUrlLoader::Result result) {
  return result == WebAppUrlLoader::Result::kUrlLoaded;
}

absl::optional<std::string> UTF16ToUTF8(base::StringPiece16 src) {
  std::string dest;
  if (!base::UTF16ToUTF8(src.data(), src.length(), &dest)) {
    return absl::nullopt;
  }
  return dest;
}

}  // namespace

InstallIsolatedAppCommand::InstallIsolatedAppCommand(
    base::StringPiece url,
    WebAppUrlLoader& url_loader,
    WebAppInstallFinalizer& install_finalizer,
    base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback)
    : lock_(std::make_unique<SharedWebContentsWithAppLock>(
          base::flat_set<AppId>{GenerateAppId("/", GURL{url})})),
      url_(url),
      url_loader_(url_loader),
      install_finalizer_(install_finalizer),
      data_retriever_(std::make_unique<WebAppDataRetriever>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  DCHECK(!callback.is_null());

  callback_ = base::BindOnce([](InstallIsolatedAppCommandResult result) {
                webapps::InstallableMetrics::TrackInstallResult(
                    result == InstallIsolatedAppCommandResult::kOk);
                return result;
              }).Then(std::move(callback));
}

void InstallIsolatedAppCommand::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

InstallIsolatedAppCommand::~InstallIsolatedAppCommand() = default;

Lock& InstallIsolatedAppCommand::lock() const {
  return *lock_;
}

void InstallIsolatedAppCommand::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto url = GURL{url_};
  if (!url.is_valid()) {
    ReportFailure();
    return;
  }

  LoadUrl(url);
}

void InstallIsolatedAppCommand::LoadUrl(GURL url) {
  DCHECK(url.is_valid());

  url_loader_.LoadUrl(url, shared_web_contents(),
                      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
                      base::BindOnce(&InstallIsolatedAppCommand::OnLoadUrl,
                                     weak_factory_.GetWeakPtr()));
}

void InstallIsolatedAppCommand::OnLoadUrl(WebAppUrlLoaderResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsUrlLoadingResultSuccess(result)) {
    ReportFailure();
    return;
  }

  CheckInstallabilityAndRetrieveManifest();
}

void InstallIsolatedAppCommand::CheckInstallabilityAndRetrieveManifest() {
  // TODO(kuragin): Fix order of calls to the data retrieve.
  //
  // The order should be:
  //  1. |GetWebAppInstallInfo|
  //  2. |CheckInstallabilityAndRetrieveManifest|
  //  3. |GetIcons|
  //
  // See install from sync command unit-test for details:
  // https://crsrc.org/c/chrome/browser/web_applications/commands/install_from_sync_command_unittest.cc;l=333;drc=ddce2fc4e67fd4500d29cbe5f4993b3fb8e4e2ba
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      shared_web_contents(),
      /*bypass_service_worker_check=*/false,
      base::BindOnce(
          &InstallIsolatedAppCommand::OnCheckInstallabilityAndRetrieveManifest,
          weak_factory_.GetWeakPtr()));
}

absl::optional<WebAppInstallInfo>
InstallIsolatedAppCommand::CreateInstallInfoFromManifest(
    const blink::mojom::Manifest& manifest,
    const GURL& manifest_url) {
  WebAppInstallInfo info;
  UpdateWebAppInfoFromManifest(manifest, manifest_url, &info);

  if (!manifest.id.has_value()) {
    return absl::nullopt;
  }

  // In other installations the best-effort encoding is fine, but for isolated
  // apps we have the opportunity to report this error.
  absl::optional<std::string> encoded_id = UTF16ToUTF8(*manifest.id);
  if (!encoded_id.has_value()) {
    return absl::nullopt;
  }

  if (*encoded_id != "/") {
    return absl::nullopt;
  }

  info.manifest_id = *encoded_id;

  return info;
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

  if (absl::optional<WebAppInstallInfo> install_info =
          CreateInstallInfoFromManifest(*opt_manifest, manifest_url);
      install_info.has_value()) {
    FinalizeInstall(*install_info);
  } else {
    ReportFailure();
  }
}

void InstallIsolatedAppCommand::FinalizeInstall(const WebAppInstallInfo& info) {
  install_finalizer_.FinalizeInstall(
      info,
      // TODO(kuragin): Add Isolated app specific install source
      // `WebappInstallSource::ISOLATED_APP_DEV_INSTALL`.
      WebAppInstallFinalizer::FinalizeOptions{
          /*install_surface=*/webapps::WebappInstallSource::MANAGEMENT_API,
      },
      base::BindOnce([](const AppId& unused_app_id,
                        webapps::InstallResultCode unused_install_result_code,
                        OsHooksErrors unused_os_hooks_errors) {
        // TODO(kuragin): Implement error handling. Current implementation of
        // the install finalizer doesn't allow to mock errors.
        //
        // See |FakeInstallFinalizer::FinalizeInstall| for details.
      })
          .Then(base::BindOnce(&InstallIsolatedAppCommand::DownloadIcons,
                               weak_factory_.GetWeakPtr())));
}

void InstallIsolatedAppCommand::DownloadIcons() {
  // TODO(kuragin): Find a way to test icons downloading and relationship with
  // icon population in the web app install info. Implement icons downloading.
  ReportSuccess();
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

void InstallIsolatedAppCommand::ReportSuccess() {
  Report(/*success=*/true);
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
