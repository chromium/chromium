// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_install_from_migrate_from_field_command.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

WebAppInstallFromMigrateFromFieldResult MapJobResult(
    MigrationTargetInstallJobResult result) {
  switch (result) {
    case MigrationTargetInstallJobResult::kAlreadyInstalled:
      return WebAppInstallFromMigrateFromFieldResult::kAlreadyInstalled;
    case MigrationTargetInstallJobResult::kSuccessInstalled:
      return WebAppInstallFromMigrateFromFieldResult::kSuccessInstalled;
    case MigrationTargetInstallJobResult::kManifestToWebAppInstallInfoError:
      return WebAppInstallFromMigrateFromFieldResult::
          kManifestToWebAppInstallInfoError;
    case MigrationTargetInstallJobResult::kInstallFromInfoFailed:
      return WebAppInstallFromMigrateFromFieldResult::kInstallFromInfoFailed;
    case MigrationTargetInstallJobResult::kUpdateFailed:
      return WebAppInstallFromMigrateFromFieldResult::kUpdateFailed;
    case MigrationTargetInstallJobResult::kWebContentsWasDestroyed:
      return WebAppInstallFromMigrateFromFieldResult::kWebContentsWasDestroyed;
  }
}

}  // namespace

WebAppInstallFromMigrateFromFieldCommand::
    WebAppInstallFromMigrateFromFieldCommand(
        base::WeakPtr<content::WebContents> web_contents,
        blink::mojom::ManifestPtr manifest,
        WebAppInstallFromMigrateFromFieldCallback callback)
    : WebAppCommand<AppLock, WebAppInstallFromMigrateFromFieldResult>(
          "WebAppInstallFromMigrateFromFieldCommand",
          AppLockDescription(GenerateAppIdFromManifest(*manifest)),
          base::BindOnce([](WebAppInstallFromMigrateFromFieldResult result) {
            base::UmaHistogramEnumeration(
                "WebApp.InstallFromMigrateFromField.Result", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          WebAppInstallFromMigrateFromFieldResult::kSystemShutdown),
      web_contents_(web_contents),
      manifest_(std::move(manifest)) {
  Observe(web_contents_.get());
}

WebAppInstallFromMigrateFromFieldCommand::
    ~WebAppInstallFromMigrateFromFieldCommand() = default;

void WebAppInstallFromMigrateFromFieldCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        WebAppInstallFromMigrateFromFieldResult::kWebContentsWasDestroyed);
    return;
  }

  bool source_installed = false;
  for (const auto& migrate_from : manifest_->migrate_from) {
    // Allow all installed apps, even source apps installed by policy that is
    // not allowed to migrate, to also install the destination app but keep it
    // hidden from the user, so as to populate the pending migration metadata
    // in the source app.
    std::optional<webapps::ManifestId> manifest_id =
        webapps::ManifestId::Create(migrate_from->id);
    if (!manifest_id.has_value()) {
      continue;
    }
    if (lock_->registrar().AppMatches(
            GenerateAppIdFromManifestId(*manifest_id),
            WebAppFilter::CanAppInstallTargetMigrationApp())) {
      source_installed = true;
      break;
    }
  }

  if (!source_installed) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        WebAppInstallFromMigrateFromFieldResult::kNoSourceAppInstalled);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());

  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();

  job_ = MigrationTargetInstallJob::CreateAndStart(
      std::move(manifest_), web_contents_, profile, data_retriever_.get(),
      &GetMutableDebugValue(), lock_.get(), lock_.get(),
      base::BindOnce(&WebAppInstallFromMigrateFromFieldCommand::OnJobFinished,
                     weak_factory_.GetWeakPtr()));
}

void WebAppInstallFromMigrateFromFieldCommand::PrimaryPageChanged(
    content::Page& page) {
  if (IsStarted()) {
    Observe(nullptr);
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        WebAppInstallFromMigrateFromFieldResult::kUserNavigated);
  }
}

void WebAppInstallFromMigrateFromFieldCommand::WebContentsDestroyed() {
  if (IsStarted()) {
    Observe(nullptr);
    CompleteAndSelfDestruct(
        CommandResult::kFailure,
        WebAppInstallFromMigrateFromFieldResult::kWebContentsWasDestroyed);
  }
}

void WebAppInstallFromMigrateFromFieldCommand::OnJobFinished(
    MigrationTargetInstallJobResult result) {
  Observe(nullptr);
  CompleteAndSelfDestruct(CommandResult::kSuccess, MapJobResult(result));
}

}  // namespace web_app
