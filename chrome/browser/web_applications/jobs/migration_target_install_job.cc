// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/migration_target_install_job.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job_result.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

std::unique_ptr<MigrationTargetInstallJob>
MigrationTargetInstallJob::CreateAndStart(
    blink::mojom::ManifestPtr manifest,
    base::WeakPtr<content::WebContents> web_contents,
    Profile* profile,
    WebAppDataRetriever* data_retriever,
    base::DictValue* debug_value,
    Lock* lock,
    WithAppResources* lock_resources,
    MigrationTargetInstallCallback callback) {
  CHECK(manifest);
  CHECK(web_contents);
  CHECK(profile);
  CHECK(debug_value);
  CHECK(lock);
  CHECK(lock_resources);
  CHECK(data_retriever);

  auto job = base::WrapUnique(new MigrationTargetInstallJob(
      std::move(manifest), web_contents, profile, data_retriever, debug_value,
      lock, lock_resources, std::move(callback)));
  job->Start();
  return job;
}

MigrationTargetInstallJob::~MigrationTargetInstallJob() = default;

MigrationTargetInstallJob::MigrationTargetInstallJob(
    blink::mojom::ManifestPtr manifest,
    base::WeakPtr<content::WebContents> web_contents,
    Profile* profile,
    WebAppDataRetriever* data_retriever,
    base::DictValue* debug_value,
    Lock* lock,
    WithAppResources* lock_resources,
    MigrationTargetInstallCallback callback)
    : manifest_(std::move(manifest)),
      web_contents_(web_contents),
      profile_(profile),
      data_retriever_(*data_retriever),
      debug_value_(*debug_value),
      lock_(*lock),
      lock_resources_(*lock_resources),
      callback_(std::move(callback)) {}

void MigrationTargetInstallJob::Start() {
  webapps::AppId app_id = GenerateAppIdFromManifestId(manifest_->id);
  if (lock_resources_->registrar().AppMatches(
          app_id, WebAppFilter::IsAppEligibleForManifestUpdate())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MigrationTargetInstallJob::Complete,
                       weak_factory_.GetWeakPtr(),
                       MigrationTargetInstallJobResult::kAlreadyInstalled));
    return;
  }
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *manifest_, data_retriever_.get(),
          /*background_installation=*/true,
          webapps::WebappInstallSource::MIGRATION, web_contents_,
          [](IconUrlSizeSet&) {},
          *debug_value_->EnsureDict("ManifestToWebAppInstallInfoJob"),
          base::BindOnce(&MigrationTargetInstallJob::
                             OnManifestToWebAppInstallInfoJobFinished,
                         weak_factory_.GetWeakPtr()));
}

void MigrationTargetInstallJob::OnManifestToWebAppInstallInfoJobFinished(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    Complete(
        MigrationTargetInstallJobResult::kManifestToWebAppInstallInfoError);
    return;
  }

  if (!web_contents_) {
    Complete(MigrationTargetInstallJobResult::kWebContentsWasDestroyed);
    return;
  }

  WebAppInstallParams install_params;
  install_params.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
  install_params.add_to_applications_menu = false;
  install_params.add_to_desktop = false;
  install_params.add_to_quick_launch_bar = false;

  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile_.get(), *debug_value_->EnsureDict("InstallFromInfoJob"),
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::MIGRATION, install_params,
      base::BindOnce(&MigrationTargetInstallJob::OnInstallFromInfoJobFinished,
                     weak_factory_.GetWeakPtr()));
  install_from_info_job_->Start(&lock_.get(), &lock_resources_.get());
}

void MigrationTargetInstallJob::OnInstallFromInfoJobFinished(
    webapps::AppId app_id,
    webapps::InstallResultCode code) {
  debug_value_->Set("install_from_info_result", base::ToString(code));
  if (webapps::IsSuccess(code)) {
    Complete(MigrationTargetInstallJobResult::kSuccessInstalled);
  } else {
    Complete(MigrationTargetInstallJobResult::kInstallFromInfoFailed);
  }
}

void MigrationTargetInstallJob::Complete(
    MigrationTargetInstallJobResult result) {
  debug_value_->Set("result", base::ToString(result));
  std::move(callback_).Run(result);
}

}  // namespace web_app
