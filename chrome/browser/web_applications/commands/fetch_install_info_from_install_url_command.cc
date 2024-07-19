// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_install_info_from_install_url_command.h"

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os, FetchInstallInfoResult result) {
  switch (result) {
    case FetchInstallInfoResult::kAppInfoObtained:
      return os << "kAppInfoObtained";
    case FetchInstallInfoResult::kWebContentsDestroyed:
      return os << "kWebContentsDestroyed";
    case FetchInstallInfoResult::kUrlLoadingFailure:
      return os << "kUrlLoadingFailure";
    case FetchInstallInfoResult::kNoValidManifest:
      return os << "kNoValidManifest";
    case FetchInstallInfoResult::kWrongManifestId:
      return os << "kWrongManifestId";
    case FetchInstallInfoResult::kFailure:
      return os << "kFailure";
  }
}

bool FetchInstallInfoFromInstallUrlCommand::
    FetchInstallInfoFromInstallUrlCommand::IsWebContentsDestroyed() {
  return lock_->shared_web_contents().IsBeingDestroyed();
}

FetchInstallInfoFromInstallUrlCommand::FetchInstallInfoFromInstallUrlCommand(
    webapps::ManifestId manifest_id,
    GURL install_url,
    std::optional<webapps::ManifestId> parent_manifest_id,
    base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback)
    : WebAppCommand<SharedWebContentsLock, std::unique_ptr<WebAppInstallInfo>>(
          "FetchInstallInfoFromInstallUrlCommand",
          SharedWebContentsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/nullptr),
      manifest_id_(manifest_id),
      install_url_(install_url),
      parent_manifest_id_(parent_manifest_id),
      install_error_log_entry_(/*background_installation=*/true,
                               webapps::WebappInstallSource::SUB_APP) {
  CHECK(manifest_id_.is_valid());
  CHECK(install_url_.is_valid());

  if (parent_manifest_id_.has_value()) {
    CHECK(parent_manifest_id_.value().is_valid());
    CHECK(url::Origin::Create(manifest_id_)
              .IsSameOriginWith(
                  url::Origin::Create(parent_manifest_id_.value())));
    CHECK_NE(parent_manifest_id_.value(), manifest_id_);
  }

  GetMutableDebugValue().Set("manifest_id", manifest_id_.spec());
  GetMutableDebugValue().Set("parent_manifest_id",
                             parent_manifest_id_.value_or(GURL("")).spec());
  GetMutableDebugValue().Set("install_url", install_url_.spec());
}

FetchInstallInfoFromInstallUrlCommand::
    ~FetchInstallInfoFromInstallUrlCommand() = default;

void FetchInstallInfoFromInstallUrlCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  lock_ = std::move(lock);

  url_loader_ = lock_->web_contents_manager().CreateUrlLoader();
  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();

  if (IsWebContentsDestroyed()) {
    CompleteCommandAndSelfDestruct(
        FetchInstallInfoResult::kWebContentsDestroyed,
        /*install_info=*/nullptr);
    return;
  }

  url_loader_->LoadUrl(
      install_url_, &lock_->shared_web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&FetchInstallInfoFromInstallUrlCommand::
                         OnWebAppUrlLoadedGetWebAppInstallInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FetchInstallInfoFromInstallUrlCommand::
    OnWebAppUrlLoadedGetWebAppInstallInfo(
        webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("url_loading_result",
                             ConvertUrlLoaderResultToString(result));

  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    install_error_log_entry_.LogUrlLoaderError(
        "OnWebAppUrlLoadedGetWebAppInstallInfo", install_url_.spec(), result);
    CompleteCommandAndSelfDestruct(FetchInstallInfoResult::kUrlLoadingFailure,
                                   /*install_info=*/nullptr);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      &lock_->shared_web_contents(),
      base::BindOnce(
          &FetchInstallInfoFromInstallUrlCommand::OnGetWebAppInstallInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

void FetchInstallInfoFromInstallUrlCommand::OnGetWebAppInstallInfo(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    CompleteCommandAndSelfDestruct(FetchInstallInfoResult::kFailure,
                                   /*install_info=*/nullptr);
    return;
  }

  install_info->install_url = install_url_;
  install_info->parent_app_manifest_id = parent_manifest_id_;

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &lock_->shared_web_contents(),
      base::BindOnce(
          &FetchInstallInfoFromInstallUrlCommand::OnManifestRetrieved,
          weak_ptr_factory_.GetWeakPtr(), std::move(install_info)));
}

void FetchInstallInfoFromInstallUrlCommand::OnManifestRetrieved(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  CHECK(web_app_info);
  if (!valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << install_url_.spec()
                 << " because it didn't have a manifest for web app";
    CompleteCommandAndSelfDestruct(FetchInstallInfoResult::kNoValidManifest,
                                   /*install_info=*/nullptr);
    return;
  }

  if (opt_manifest) {
    UpdateWebAppInfoFromManifest(*opt_manifest, web_app_info.get());
  }

  webapps::AppId app_id = GenerateAppIdFromManifestId(
      web_app_info->manifest_id(), web_app_info->parent_app_manifest_id);
  const webapps::AppId expected_app_id = GenerateAppIdFromManifestId(
      manifest_id_, web_app_info->parent_app_manifest_id);
  if (app_id != expected_app_id) {
    install_error_log_entry_.LogExpectedAppIdError(
        "OnManifestRetrieved", web_app_info->start_url().spec(), app_id,
        expected_app_id);
    CompleteCommandAndSelfDestruct(FetchInstallInfoResult::kWrongManifestId,
                                   /*install_info=*/nullptr);
    return;
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();
  IconUrlSizeSet icon_urls = GetValidIconUrlsToDownload(*web_app_info);

  data_retriever_->GetIcons(
      &lock_->shared_web_contents(), std::move(icon_urls), skip_page_favicons,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(&FetchInstallInfoFromInstallUrlCommand::OnIconsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));
}

void FetchInstallInfoFromInstallUrlCommand::OnIconsRetrieved(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  CHECK(web_app_info);
  PopulateProductIcons(web_app_info.get(), &icons_map);
  PopulateOtherIcons(web_app_info.get(), icons_map);
  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  CompleteCommandAndSelfDestruct(FetchInstallInfoResult::kAppInfoObtained,
                                 std::move(web_app_info));
}

void FetchInstallInfoFromInstallUrlCommand::CompleteCommandAndSelfDestruct(
    FetchInstallInfoResult result,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  GetMutableDebugValue().Set("command_result", base::ToString(result));

  CommandResult command_result = [&] {
    switch (result) {
      case FetchInstallInfoResult::kAppInfoObtained:
        return CommandResult::kSuccess;
      default:
        return CommandResult::kFailure;
    }
  }();

  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo) &&
      install_error_log_entry_.HasErrorDict()) {
    command_manager()->LogToInstallManager(
        install_error_log_entry_.TakeErrorDict());
  }

  CompleteAndSelfDestruct(command_result, std::move(install_info));
}

}  // namespace web_app
