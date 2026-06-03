// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

namespace {

constexpr static char kGeneratedInstallPagePath[] =
    "/.well-known/_generated_install_page.html";

bool IsUrlLoadingResultSuccess(webapps::WebAppUrlLoaderResult result) {
  return result == webapps::WebAppUrlLoaderResult::kUrlLoaded;
}

}  // namespace

// static
std::unique_ptr<PrepareInstallInfoJob> PrepareInstallInfoJob::CreateAndStart(
    Profile& profile,
    IwaSourceWithMode source,
    IwaOperation operation,
    std::optional<IwaVersion> expected_version,
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    std::unique_ptr<webapps::WebAppUrlLoader> loader,
    ResultCallback callback) {
  auto job = base::WrapUnique(new PrepareInstallInfoJob(
      profile, std::move(source), std::move(operation),
      std::move(expected_version), std::move(url_info),
      std::move(data_retriever)));
  job->Start(std::move(loader), std::move(callback));
  return job;
}

PrepareInstallInfoJob::~PrepareInstallInfoJob() = default;

PrepareInstallInfoJob::PrepareInstallInfoJob(
    Profile& profile,
    IwaSourceWithMode source,
    IwaOperation operation,
    std::optional<IwaVersion> expected_version,
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : profile_(profile),
      source_(std::move(source)),
      operation_(std::move(operation)),
      expected_version_(std::move(expected_version)),
      url_info_(std::move(url_info)),
      data_retriever_(std::move(data_retriever)) {
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(&profile));
  webapps::InstallableManager::CreateForWebContents(web_contents_.get());
}

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
  NonInstalledBundleInspectionContext::CreateForWebContents(
      web_contents_.get(), source_, operation_);

  GURL install_page_url =
      url_info_.origin().GetURL().Resolve(kGeneratedInstallPagePath);

  content::NavigationController::LoadURLParams load_params(install_page_url);
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  load_params.reload_type = content::ReloadType::BYPASSING_CACHE;

  url_loader_->LoadUrl(
      std::move(load_params), web_contents_.get(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&PrepareInstallInfoJob::OnLoadInstallUrl,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void PrepareInstallInfoJob::OnLoadInstallUrl(
    base::OnceClosure next_step_callback,
    webapps::WebAppUrlLoaderResult result) {
  if (!IsUrlLoadingResultSuccess(result)) {
    ReportFailure(
        Error::kCantLoadInstallUrl,
        base::StrCat({"Error during URL loading: ", base::ToString(result)}));
    return;
  }

  std::move(next_step_callback).Run();
}

void PrepareInstallInfoJob::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback) {
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &PrepareInstallInfoJob::OnCheckInstallabilityAndRetrieveManifest,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void PrepareInstallInfoJob::OnCheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback,
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (error_code != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    ReportFailure(Error::kAppIsNotInstallable,
                  base::StrCat({"App is not installable: ",
                                webapps::GetErrorMessage(error_code), "."}));
    return;
  }

  CHECK(valid_manifest_for_web_app);

  if (!opt_manifest) {
    ReportFailure(Error::kAppIsNotInstallable, "Manifest is null.");
    return;
  }

  if (opt_manifest->manifest_url.is_empty()) {
    ReportFailure(Error::kAppIsNotInstallable, "Manifest URL is empty.");
    return;
  }

  CHECK(!blink::IsEmptyManifest(opt_manifest));

  std::move(next_step_callback).Run(std::move(opt_manifest));
}

void PrepareInstallInfoJob::ValidateManifestAndGetVersion(
    base::OnceCallback<void(IwaVersion)> next_step_callback,
    blink::mojom::ManifestPtr manifest) {
  const GURL& manifest_url = manifest->manifest_url;

  if (!manifest->version.has_value()) {
    ReportFailure(Error::kCantValidateManifest,
                  "Manifest `version` is not present. manifest_url: " +
                      manifest_url.possibly_invalid_spec());
    return;
  }

  std::string version_string = base::UTF16ToUTF8(*manifest->version);

  auto iwa_version_result = IwaVersion::Create(version_string);
  if (!iwa_version_result.has_value()) {
    ReportFailure(
        Error::kCantValidateManifest,
        base::StrCat(
            {"Failed to parse `version` from the manifest. Got: ",
             version_string,
             ". It must be in the form `x.y.z`, where `x`, `y`, and `z` "
             "are numbers without leading zeros."}));
    return;
  }

  IwaVersion iwa_version = std::move(*iwa_version_result);

  if (expected_version_.has_value() && *expected_version_ != iwa_version) {
    ReportFailure(
        Error::kCantValidateManifest,
        "Expected version (" + expected_version_->GetString() +
            ") does not match the version provided in the manifest (" +
            iwa_version.GetString() + ")");
    return;
  }

  // Recommend to use "/" for manifest id and not empty manifest id because the
  // manifest parser does additional work on resolving manifest id taking
  // `start_url` into account. (See https://w3c.github.io/manifest/#id-member on
  // how the manifest parser resolves the `id` field).
  //
  // It is required for Isolated Web Apps to have app id based on origin of the
  // application and do not include other information in order to be able to
  // identify Isolated Web Apps by origin because there is always only 1 app per
  // origin.
  std::string encoded_id = manifest->id.GetPath();
  if (encoded_id != "/") {
    ReportFailure(
        Error::kCantValidateManifest,
        R"(Manifest `id` must be "/". Resolved manifest id: )" + encoded_id);
    return;
  }

  url::Origin origin = url_info_.origin();
  if (manifest->scope != origin.GetURL()) {
    ReportFailure(Error::kCantValidateManifest,
                  base::StrCat({"Scope should resolve to the origin. scope: ",
                                manifest->scope.possibly_invalid_spec(),
                                ", origin: ", origin.Serialize()}));
    return;
  }

  manifest_ = std::move(manifest);
  std::move(next_step_callback).Run(std::move(iwa_version));
}

void PrepareInstallInfoJob::ParseInstallInfoFromManifest(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    const IwaVersion parsed_version) {
  WebAppInstallInfoConstructOptions construct_options;
  construct_options.fail_all_if_any_fail = true;
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *manifest_, *data_retriever_.get(), /*background_installation=*/false,
          webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER,
          web_contents_->GetWeakPtr(), [](IconUrlSizeSet& icon_url_size_set) {},
          manifest_to_info_debug_data_,
          base::BindOnce(
              &PrepareInstallInfoJob::OnGettingInstallInfoFromManifest,
              weak_factory_.GetWeakPtr(), std::move(next_step_callback),
              parsed_version),
          construct_options);
}

void PrepareInstallInfoJob::OnGettingInstallInfoFromManifest(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    const IwaVersion parsed_version,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    ReportFailure(Error::kCantRetrieveIcons, "Install info is null.");
    return;
  }

  install_info->set_isolated_web_app_version(std::move(parsed_version));

  install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

  if (install_info->title.empty()) {
    ReportFailure(
        Error::kCantValidateManifest,
        base::StrCat({"App manifest must have either 'name' or 'short_name'. "
                      "manifest_url: ",
                      install_info->manifest_url.possibly_invalid_spec()}));
    return;
  }

  if (install_info->is_generated_icon) {
    ReportFailure(Error::kCantRetrieveIcons,
                  "Error during icon downloading, stopping installation.");
    return;
  }

  std::move(next_step_callback).Run(std::move(*install_info));
}

void PrepareInstallInfoJob::FinishJob(WebAppInstallInfo info) {
  CHECK(!expected_version_ ||
        *expected_version_ == info.isolated_web_app_version());
  url_loader_.reset();
  std::move(callback_).Run(std::move(info));
}

}  // namespace web_app
