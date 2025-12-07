// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"

#include "base/version.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/types/source.h"

namespace web_app {

// static
std::unique_ptr<PrepareInstallInfoJob> PrepareInstallInfoJob::CreateAndStart(
    Profile& profile,
    IwaSourceWithMode source,
    std::optional<IwaVersion> expected_version,
    content::WebContents& web_contents,
    IsolatedWebAppInstallCommandHelper& command_helper,
    std::unique_ptr<webapps::WebAppUrlLoader> loader,
    ResultCallback callback) {
  auto job = base::WrapUnique(new PrepareInstallInfoJob(
      profile, std::move(source), std::move(expected_version), web_contents,
      command_helper));
  job->Start(std::move(loader), std::move(callback));
  return job;
}

PrepareInstallInfoJob::~PrepareInstallInfoJob() = default;

PrepareInstallInfoJob::PrepareInstallInfoJob(
    Profile& profile,
    IwaSourceWithMode source,
    std::optional<IwaVersion> expected_version,
    content::WebContents& web_contents,
    IsolatedWebAppInstallCommandHelper& command_helper)
    : profile_(profile),
      source_(std::move(source)),
      expected_version_(std::move(expected_version)),
      web_contents_(web_contents),
      command_helper_(command_helper) {}

void PrepareInstallInfoJob::Start(
    std::unique_ptr<webapps::WebAppUrlLoader> loader,
    ResultCallback callback) {
  CHECK(!callback_);

  callback_ = std::move(callback);
  url_loader_ = std::move(loader);

  // clang-format off
  RunChainedWeakCallbacks(
      weak_factory_.GetWeakPtr(),
      &PrepareInstallInfoJob::LoadInstallUrl,
      &PrepareInstallInfoJob::CheckInstallabilityAndRetrieveManifest,
      &PrepareInstallInfoJob::ValidateManifestAndGetVersion,
      &PrepareInstallInfoJob::ParseInstallInfoFromManifest,
      &PrepareInstallInfoJob::FinishJob);
  // clang-format on
}

void PrepareInstallInfoJob::ReportFailure(Error error,
                                          const std::string& message) {
  url_loader_.reset();
  std::move(callback_).Run(
      base::unexpected(Failure{.error = error, .message = message}));
}

void PrepareInstallInfoJob::LoadInstallUrl(
    base::OnceClosure next_step_callback) {
  command_helper_->LoadInstallUrl(
      source_, *web_contents_, *url_loader_.get(),
      base::BindOnce(&PrepareInstallInfoJob::RunNextStepOnSuccess<void>,
                     weak_factory_.GetWeakPtr(), std::move(next_step_callback),
                     Error::kCantLoadInstallUrl));
}

void PrepareInstallInfoJob::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback) {
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_,
      base::BindOnce(&PrepareInstallInfoJob::RunNextStepOnSuccess<
                         blink::mojom::ManifestPtr>,
                     weak_factory_.GetWeakPtr(), std::move(next_step_callback),
                     Error::kAppIsNotInstallable));
}

void PrepareInstallInfoJob::ValidateManifestAndGetVersion(
    base::OnceCallback<void(IwaVersion)> next_step_callback,
    blink::mojom::ManifestPtr manifest) {
  base::expected<IwaVersion, std::string> version_validation_result =
      command_helper_->ValidateManifestAndGetVersion(expected_version_,
                                                     *manifest);
  manifest_ = std::move(manifest);
  RunNextStepOnSuccess(std::move(next_step_callback),
                       Error::kCantValidateManifest,
                       std::move(version_validation_result));
}

void PrepareInstallInfoJob::ParseInstallInfoFromManifest(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    const IwaVersion parsed_version) {
  command_helper_->RetrieveInstallInfoWithIconsFromManifest(
      *manifest_, *web_contents_, std::move(parsed_version),
      base::BindOnce(
          &PrepareInstallInfoJob::RunNextStepOnSuccess<WebAppInstallInfo>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback),
          Error::kCantRetrieveIcons));
}

void PrepareInstallInfoJob::FinishJob(WebAppInstallInfo info) {
  CHECK(!expected_version_ ||
        *expected_version_ == info.isolated_web_app_version());
  url_loader_.reset();
  std::move(callback_).Run(std::move(info));
}

}  // namespace web_app
