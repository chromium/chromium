// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_app_from_verified_manifest_command.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

// TODO(crbug.com/40273612): Find a better way to do Lacros testing so that we
// don't have to pass localhost into the allowlist. Allowlisted host must be
// from a Google server.
constexpr auto kHostAllowlist = base::MakeFixedFlatSet<std::string_view>(
    {"googleusercontent.com", "gstatic.com", "youtube.com",
     "127.0.0.1" /*FOR TESTING*/});

bool HasRequiredManifestFields(const blink::mojom::ManifestPtr& manifest) {
  if (!manifest->has_valid_specified_start_url) {
    return false;
  }

  if (!manifest->short_name.has_value() && !manifest->name.has_value()) {
    return false;
  }

  return true;
}

}  // namespace

InstallAppFromVerifiedManifestCommand::InstallAppFromVerifiedManifestCommand(
    webapps::WebappInstallSource install_source,
    GURL document_url,
    GURL verified_manifest_url,
    std::string verified_manifest_contents,
    webapps::AppId expected_id,
    bool is_diy_app,
    std::optional<WebAppInstallParams> install_params,
    OnceInstallCallback callback)
    : WebAppCommand<SharedWebContentsLock,
                    const webapps::AppId&,
                    webapps::InstallResultCode>(
          "InstallAppFromVerifiedManifestCommand",
          SharedWebContentsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(webapps::AppId(),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      install_source_(install_source),
      document_url_(std::move(document_url)),
      verified_manifest_url_(std::move(verified_manifest_url)),
      verified_manifest_contents_(std::move(verified_manifest_contents)),
      expected_id_(std::move(expected_id)),
      is_diy_app_(is_diy_app),
      install_params_(std::move(install_params)) {
  if (install_params_) {
    // Not every `install_params` option has an effect, check that unused params
    // are not set.
    CHECK_EQ(install_params->install_state,
             proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    CHECK(install_params->fallback_start_url.is_empty());
    CHECK(!install_params->fallback_app_name.has_value());
  }

  GetMutableDebugValue().Set("document_url", document_url_.spec());
  GetMutableDebugValue().Set("verified_manifest_url",
                             verified_manifest_url_.spec());
  GetMutableDebugValue().Set("expected_id", expected_id_);
  GetMutableDebugValue().Set("verified_manifest_contents",
                             verified_manifest_contents_);
  GetMutableDebugValue().Set("has_install_params", !!install_params_);
}

InstallAppFromVerifiedManifestCommand::
    ~InstallAppFromVerifiedManifestCommand() = default;

void InstallAppFromVerifiedManifestCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  url_loader_ = web_contents_lock_->web_contents_manager().CreateUrlLoader();
  data_retriever_ =
      web_contents_lock_->web_contents_manager().CreateDataRetriever();
  url_loader_->LoadUrl(
      GURL(url::kAboutBlankURL), &web_contents_lock_->shared_web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&InstallAppFromVerifiedManifestCommand::OnAboutBlankLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallAppFromVerifiedManifestCommand::OnAboutBlankLoaded(
    webapps::WebAppUrlLoaderResult result) {
  // The shared web contents must have been reset to about:blank before command
  // execution.
  DCHECK_EQ(web_contents_lock_->shared_web_contents().GetURL(),
            GURL(url::kAboutBlankURL));

  web_contents_lock_->shared_web_contents()
      .GetPrimaryMainFrame()
      ->GetRemoteInterfaces()
      ->GetInterface(manifest_manager_.BindNewPipeAndPassReceiver());
  manifest_manager_.set_disconnect_handler(
      base::BindOnce(&InstallAppFromVerifiedManifestCommand::Abort,
                     weak_ptr_factory_.GetWeakPtr(), CommandResult::kFailure,
                     webapps::InstallResultCode::kWebContentsDestroyed));

  manifest_manager_->ParseManifestFromString(
      document_url_, verified_manifest_url_, verified_manifest_contents_,
      base::BindOnce(&InstallAppFromVerifiedManifestCommand::OnManifestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallAppFromVerifiedManifestCommand::OnManifestParsed(
    blink::mojom::ManifestPtr manifest) {
  // Note that most errors during parsing (e.g. errors to do with parsing a
  // particular field) are silently ignored. As long as the manifest is valid
  // JSON and contains a valid start_url and name, installation will proceed.
  if (blink::IsEmptyManifest(manifest) ||
      !HasRequiredManifestFields(manifest)) {
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  if (manifest->manifest_url != verified_manifest_url_) {
    mojo::ReportBadMessage("Returned manifest has incorrect manifest URL");
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  GetMutableDebugValue().Set("manifest_parsed", true);
  web_app_info_ =
      std::make_unique<WebAppInstallInfo>(manifest->id, manifest->start_url);
  web_app_info_->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info_->is_diy_app = is_diy_app_;

  UpdateWebAppInfoFromManifest(*manifest, web_app_info_.get());

  if (install_params_) {
    // TODO(crbug.com/354981650): Remove this call.
    ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_info_);
  }

  IconUrlSizeSet icon_urls = GetValidIconUrlsToDownload(*web_app_info_);
  base::EraseIf(icon_urls, [](const IconUrlWithSize& url_with_size) {
    for (const auto& allowed_host : kHostAllowlist) {
      const GURL& icon_url = url_with_size.url;
      if (icon_url.DomainIs(allowed_host)) {
        // Found a match, don't erase this url!
        return false;
      }
    }
    // No matches, erase this url!
    return true;
  });

  if (icon_urls.empty()) {
    // Abort if there are no icons to download, so we can distinguish this case
    // from having icons but failing to download them.
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kNoValidIconsInManifest);
    return;
  }

  data_retriever_->GetIcons(
      &web_contents_lock_->shared_web_contents(), std::move(icon_urls),
      /*skip_page_favicons=*/true,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(&InstallAppFromVerifiedManifestCommand::OnIconsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallAppFromVerifiedManifestCommand::OnIconsRetrieved(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK(web_app_info_);

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  if (web_app_info_->is_generated_icon) {
    // PopulateProductIcons sets is_generated_icon if it had to generate a
    // product icon due a lack of successfully downloaded product icons. In
    // this case, abort the installation and report the error.
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kIconDownloadingFailed);
    return;
  }

  PopulateOtherIcons(web_app_info_.get(), icons_map);

  webapps::AppId app_id =
      GenerateAppIdFromManifestId(web_app_info_->manifest_id());

  if (app_id != expected_id_) {
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  app_lock_ = std::make_unique<SharedWebContentsWithAppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(web_contents_lock_), *app_lock_, {app_id},
      base::BindOnce(&InstallAppFromVerifiedManifestCommand::OnAppLockAcquired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallAppFromVerifiedManifestCommand::OnAppLockAcquired() {
  CHECK(app_lock_);
  CHECK(app_lock_->IsGranted());
  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_source_);
  finalize_options.add_to_quick_launch_bar = false;
  finalize_options.overwrite_existing_manifest_fields = false;

  if (install_params_) {
    ApplyParamsToFinalizeOptions(*install_params_, finalize_options);
  }

  // TODO(crbug.com/40197834): apply host_allowlist instead of disabling origin
  // association validate for all origins.
  finalize_options.skip_origin_association_validation = true;

  app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(&InstallAppFromVerifiedManifestCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallAppFromVerifiedManifestCommand::OnInstallFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  CompleteAndSelfDestruct(webapps::IsSuccess(code) ? CommandResult::kSuccess
                                                   : CommandResult::kFailure,
                          app_id, code);
}

void InstallAppFromVerifiedManifestCommand::Abort(
    CommandResult result,
    webapps::InstallResultCode code) {
  GetMutableDebugValue().Set("error_code", base::ToString(code));
  CompleteAndSelfDestruct(result, webapps::AppId(), code);
}

}  // namespace web_app
