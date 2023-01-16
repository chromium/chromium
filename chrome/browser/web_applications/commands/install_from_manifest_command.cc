// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_manifest_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/url_constants.h"

namespace web_app {

InstallFromManifestCommand::InstallFromManifestCommand(
    webapps::WebappInstallSource install_source,
    GURL document_url,
    GURL manifest_url,
    std::string manifest_contents,
    AppId expected_id,
    OnceInstallCallback callback)
    : WebAppCommandTemplate<SharedWebContentsLock>(
          "InstallFromManifestCommand"),
      install_source_(install_source),
      document_url_(std::move(document_url)),
      manifest_url_(std::move(manifest_url)),
      manifest_contents_(std::move(manifest_contents)),
      expected_id_(std::move(expected_id)),
      install_callback_(std::move(callback)),
      web_contents_lock_description_(
          std::make_unique<SharedWebContentsLockDescription>()) {}

InstallFromManifestCommand::~InstallFromManifestCommand() = default;

LockDescription& InstallFromManifestCommand::lock_description() const {
  DCHECK(web_contents_lock_description_ || app_lock_description_);

  if (app_lock_description_) {
    return *app_lock_description_;
  }

  return *web_contents_lock_description_;
}

void InstallFromManifestCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  // The shared web contents must have been reset to about:blank before command
  // execution.
  DCHECK_EQ(web_contents_lock_->shared_web_contents().GetURL(),
            GURL(url::kAboutBlankURL));

  web_contents_lock_->shared_web_contents()
      .GetPrimaryMainFrame()
      ->GetRemoteInterfaces()
      ->GetInterface(manifest_manager_.BindNewPipeAndPassReceiver());
  manifest_manager_.set_disconnect_handler(
      base::BindOnce(&InstallFromManifestCommand::Abort,
                     weak_ptr_factory_.GetWeakPtr(), CommandResult::kFailure,
                     webapps::InstallResultCode::kWebContentsDestroyed));

  manifest_manager_->ParseManifestFromString(
      document_url_, manifest_url_, manifest_contents_,
      base::BindOnce(&InstallFromManifestCommand::OnManifestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::Value InstallFromManifestCommand::ToDebugValue() const {
  base::Value::Dict debug_value;
  debug_value.Set("document_url", document_url_.spec());
  debug_value.Set("manifest_url", manifest_url_.spec());
  debug_value.Set("manifest_contents", manifest_contents_);
  debug_value.Set("manifest_parsed", manifest_parsed_);
  return base::Value(std::move(debug_value));
}

void InstallFromManifestCommand::OnShutdown() {
  Abort(CommandResult::kShutdown,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

void InstallFromManifestCommand::OnSyncSourceRemoved() {}

void InstallFromManifestCommand::OnManifestParsed(
    blink::mojom::ManifestPtr manifest) {
  // Note that most errors during parsing (e.g. errors to do with parsing a
  // particular field) are silently ignored. As long as the manifest is valid
  // JSON and contains a valid start_url, installation will proceed.
  if (blink::IsEmptyManifest(manifest) || !manifest->start_url.is_valid()) {
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  manifest_parsed_ = true;
  web_app_info_ = std::make_unique<WebAppInstallInfo>();
  web_app_info_->user_display_mode = mojom::UserDisplayMode::kStandalone;

  UpdateWebAppInfoFromManifest(*manifest, manifest_url_, web_app_info_.get());

  // Generates a fallback icon for the app.
  // TODO(crbug.com/1402583): Download and use real icons.
  PopulateProductIcons(web_app_info_.get(), nullptr);

  AppId app_id =
      GenerateAppId(web_app_info_->manifest_id, web_app_info_->start_url);

  if (app_id != expected_id_) {
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  app_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(web_contents_lock_), {app_id},
          base::BindOnce(&InstallFromManifestCommand::OnAppLockAcquired,
                         weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromManifestCommand::OnAppLockAcquired(
    std::unique_ptr<SharedWebContentsWithAppLock> app_lock) {
  app_lock_ = std::move(app_lock);
  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_source_);
  finalize_options.add_to_quick_launch_bar = false;
  finalize_options.overwrite_existing_manifest_fields = false;

  app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(&InstallFromManifestCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromManifestCommand::OnInstallFinalized(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  SignalCompletionAndSelfDestruct(
      webapps::IsSuccess(code) ? CommandResult::kSuccess
                               : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id, code));
}

void InstallFromManifestCommand::Abort(CommandResult result,
                                       webapps::InstallResultCode code) {
  SignalCompletionAndSelfDestruct(
      result, base::BindOnce(std::move(install_callback_), AppId(), code));
}

}  // namespace web_app
