// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_install_from_manifest_command.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_metrics.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/jobs/finalize_install_or_update_job.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace {
constexpr webapps::WebappInstallSource kInstallSource =
    webapps::WebappInstallSource::WEB_INSTALL;
}  // namespace

namespace web_app {

WebInstallFromManifestCommand::WebInstallFromManifestCommand(
    Profile& profile,
    blink::mojom::ManifestPtr manifest,
    const GURL& manifest_url,
    base::WeakPtr<content::WebContents> initiating_web_contents,
    base::WeakPtr<content::Page> initiating_page,
    const GURL& requesting_page_url,
    WebAppInstallDialogCallback dialog_callback,
    WebInstallFromManifestCommandCallback installed_callback)
    : WebAppCommand<SharedWebContentsLock,
                    const webapps::AppId&,
                    webapps::InstallResultCode>(
          "WebInstallFromManifestCommand",
          SharedWebContentsLockDescription(),
          base::BindOnce(
              [](WebInstallFromManifestCommandCallback callback,
                 const webapps::AppId& app_id,
                 webapps::InstallResultCode code) {
                RecordInstallMetrics(InstallCommand::kInstallFromManifestUrl,
                                     WebAppType::kCraftedApp, code,
                                     kInstallSource);
                std::move(callback).Run(app_id, code);
              },
              std::move(installed_callback)),
          /*args_for_shutdown=*/
          std::make_tuple(webapps::AppId(),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      profile_(profile),
      manifest_(std::move(manifest)),
      manifest_url_(manifest_url),
      initiating_web_contents_(initiating_web_contents),
      initiating_page_(std::move(initiating_page)),
      requesting_page_url_(requesting_page_url),
      dialog_callback_(std::move(dialog_callback)) {
  if (initiating_web_contents_) {
    Observe(initiating_web_contents_.get());
  }
  GetMutableDebugValue().Set("manifest_url_param", manifest_url_.spec());
  GetMutableDebugValue().Set("installed_by", requesting_page_url_.spec());
}

WebInstallFromManifestCommand::~WebInstallFromManifestCommand() = default;

content::WebContents* WebInstallFromManifestCommand::GetInstallingWebContents(
    base::PassKey<WebAppCommandManager>) {
  return initiating_web_contents_.get();
}

void WebInstallFromManifestCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  webapps::InstallableMetrics::TrackInstallEvent(kInstallSource);

  // The dialog is anchored to the initiating page and reads its origin; bail
  // out if the page went away or navigated to a new document while the shared
  // web contents lock was being acquired (between being queued and getting a
  // lock).
  if (page_changed_before_start_ || IsInitiatingPageGone()) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }

  // The Web Install API only installs crafted apps, which must declare at least
  // one icon in their manifest.
  if (manifest_->icons.empty()) {
    Abort(webapps::InstallResultCode::kNoValidIconsInManifest);
    return;
  }

  data_retriever_ =
      web_contents_lock_->web_contents_manager().CreateDataRetriever();

  // Convert manifest to WebAppInstallInfo and download icons via the shared
  // web contents (about:blank context — no user session cookies).
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *manifest_, *data_retriever_.get(),
          /*background_installation=*/true, kInstallSource,
          web_contents_lock_->shared_web_contents().GetWeakPtr(),
          [](IconUrlSizeSet& icon_url_size_set) {}, GetMutableDebugValue(),
          base::BindOnce(&WebInstallFromManifestCommand::
                             OnWebAppInstallInfoCreatedShowDialog,
                         weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromManifestCommand::PrimaryPageChanged(content::Page& page) {
  // The primary page changed on the initiating WebContents. This means a
  // cross-document navigation committed. The install context is no longer
  // valid, so the install must not proceed.
  if (!IsStarted()) {
    // Command is queued but hasn't acquired its lock yet. Record the event so
    // StartWithLock can abort safely.
    page_changed_before_start_ = true;
    return;
  }
  Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
}

void WebInstallFromManifestCommand::OnWebAppInstallInfoCreatedShowDialog(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  CHECK(install_info);
  // Critical origin-spoofing check: if the initiating page navigated to a new
  // document while we were downloading icons, the dialog would read the wrong
  // origin. Abort before showing it.
  if (IsInitiatingPageGone()) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }

  web_app_info_ = std::move(install_info);
  web_app_info_->manifest_url = manifest_url_;
  // Required to display the dialog's "requesting origin" subtitle.
  web_app_info_->installed_by = requesting_page_url_;

  // Upgrade lock to include app lock for finalization.
  std::optional<webapps::ManifestId> opt_manifest_id =
      webapps::ManifestId::Create(manifest_->id);
  // `manifest_->id` is guaranteed valid: it derives from `manifest_url_`
  // which is validated as an HTTPS/localhost URL, and then used as the
  // document_url during parsing.
  CHECK(opt_manifest_id.has_value());

  CHECK(!shared_web_contents_with_app_lock_);
  shared_web_contents_with_app_lock_ =
      std::make_unique<SharedWebContentsWithAppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock_), *shared_web_contents_with_app_lock_,
      {GenerateAppIdFromManifestId(*opt_manifest_id)},
      base::BindOnce(&WebInstallFromManifestCommand::OnAppLockAcquired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromManifestCommand::OnAppLockAcquired() {
  CHECK(shared_web_contents_with_app_lock_->IsGranted());

  // The app lock is now held, but the initiating page may have gone away or
  // navigated to a new document while acquiring it.
  // `WebAppInstallFlowDialogDelegate` dereferences the WebContents without a
  // null check and reads the page origin, so guard here.
  if (IsInitiatingPageGone()) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }
  // Show the install dialog on the initiating page.
  // TODO(crbug.com/415825168): Support detailed install dialog for background
  // installs. For now, pass `nullptr` to the screenshot_fetcher which will
  // always show the simple dialog.
  std::move(dialog_callback_)
      .Run(
          /*screenshot_fetcher=*/nullptr, initiating_web_contents_.get(),
          std::move(web_app_info_),
          base::BindOnce(
              &WebInstallFromManifestCommand::OnInstallDialogCompleted,
              weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromManifestCommand::OnInstallDialogCompleted(
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceResultCallback result_callback) {
  acceptance_result_callback_ = std::move(result_callback);
  if (IsInitiatingPageGone()) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }
  if (!user_accepted) {
    Abort(webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  web_app_info_ = std::move(web_app_info);
  web_app_info_->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  // Required to persist to web app database.
  web_app_info_->installed_by = requesting_page_url_;

  FinalizeJobOptions finalize_options(kInstallSource);
  finalize_options.install_state =
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
  finalize_options.overwrite_existing_manifest_fields = true;
  finalize_options.add_to_applications_menu = true;
  finalize_options.add_to_desktop = true;

  install_job_ = std::make_unique<FinalizeInstallOrUpdateJob>(
      profile_.get(), shared_web_contents_with_app_lock_.get(),
      shared_web_contents_with_app_lock_.get(), *web_app_info_,
      finalize_options);

  install_job_->Start(
      base::BindOnce(&WebInstallFromManifestCommand::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallFromManifestCommand::OnAppInstalled(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  install_job_.reset();
  if (IsInitiatingPageGone()) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    Abort(code);
    return;
  }

  Observe(nullptr);

  RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                    kInstallSource);
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code),
                                                  kInstallSource);
  MeasureUserInstalledAppHistogram(code);

  // Launch the app after installation.
  base::OnceClosure launch_closure = base::BindOnce(
      [](base::WeakPtr<WebAppCommandScheduler> scheduler,
         webapps::AppId app_id) {
        if (scheduler) {
          scheduler->LaunchApp(app_id, std::nullopt, base::DoNothing(),
                               apps::LaunchSource::kFromWebInstallApi,
                               FROM_HERE);
        }
      },
      WebAppProvider::GetForWebApps(&profile_.get())->scheduler().GetWeakPtr(),
      app_id);

  if (acceptance_result_callback_) {
    std::move(acceptance_result_callback_).Run(true, std::move(launch_closure));
  }

  const GURL manifest_id =
      shared_web_contents_with_app_lock_->registrar().GetComputedManifestId(
          app_id);
  CHECK(manifest_->id == manifest_id);

  CompleteAndSelfDestruct(CommandResult::kSuccess, app_id, code);
}

void WebInstallFromManifestCommand::Abort(webapps::InstallResultCode code) {
  Observe(nullptr);
  GetMutableDebugValue().Set("result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(/*result=*/false,
                                                  kInstallSource);
  MeasureUserInstalledAppHistogram(code);
  if (acceptance_result_callback_) {
    std::move(acceptance_result_callback_).Run(false, base::DoNothing());
  }

  CompleteAndSelfDestruct(CommandResult::kFailure, webapps::AppId(), code);
}

bool WebInstallFromManifestCommand::IsInitiatingPageGone() const {
  // The page WeakPtr is invalidated when the Page is destroyed. If it's still
  // valid, also check it is still the primary page — it could have been moved
  // to the back-forward cache.
  return !initiating_page_ || !initiating_page_->IsPrimary();
}

void WebInstallFromManifestCommand::MeasureUserInstalledAppHistogram(
    webapps::InstallResultCode code) {
  if (!web_app_info_) {
    return;
  }

  bool is_new_success_install = webapps::IsNewInstall(code);
  base::UmaHistogramBoolean("WebApp.NewCraftedAppInstalled.ByUser",
                            is_new_success_install);
}

}  // namespace web_app
