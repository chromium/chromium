// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_data_retriever.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

FakeDataRetriever::FakeDataRetriever() = default;

FakeDataRetriever::~FakeDataRetriever() {
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

void FakeDataRetriever::GetWebAppInstallInfo(
    content::WebContents* web_contents,
    GetWebAppInstallInfoCallback callback) {
  DCHECK(web_contents);

  completion_callback_ =
      base::BindOnce(std::move(callback), std::move(web_app_info_));
  ScheduleCompletionCallback();
}

void FakeDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    bool bypass_service_worker_check,
    CheckInstallabilityCallback callback,
    absl::optional<webapps::InstallableParams> params) {
  completion_callback_ =
      base::BindOnce(std::move(callback), manifest_.Clone(), manifest_url_,
                     /*valid_manifest_for_web_app=*/true, error_code_);
  ScheduleCompletionCallback();
}

void FakeDataRetriever::GetIcons(content::WebContents* web_contents,
                                 const base::flat_set<GURL>& icon_urls,
                                 bool skip_page_favicons,
                                 bool fail_all_if_any_fail,
                                 GetIconsCallback callback) {
  completion_callback_ =
      base::BindOnce(std::move(callback), icons_downloaded_result_,
                     std::move(icons_map_), std::move(icons_http_results_));
  ScheduleCompletionCallback();

  icons_map_.clear();
  icons_http_results_.clear();
}

void FakeDataRetriever::SetRendererWebAppInstallInfo(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  web_app_info_ = std::move(web_app_info);
}

void FakeDataRetriever::SetEmptyRendererWebAppInstallInfo() {
  SetRendererWebAppInstallInfo(std::make_unique<WebAppInstallInfo>());
}

void FakeDataRetriever::SetManifest(blink::mojom::ManifestPtr manifest,
                                    webapps::InstallableStatusCode error_code,
                                    GURL manifest_url) {
  manifest_ = std::move(manifest);
  error_code_ = error_code;
  manifest_url_ = std::move(manifest_url);
}

void FakeDataRetriever::SetIcons(IconsMap icons_map) {
  icons_map_ = std::move(icons_map);
}

void FakeDataRetriever::SetIconsDownloadedResult(IconsDownloadedResult result) {
  icons_downloaded_result_ = result;
}

void FakeDataRetriever::SetDownloadedIconsHttpResults(
    DownloadedIconsHttpResults icons_http_results) {
  icons_http_results_ = std::move(icons_http_results);
}

void FakeDataRetriever::SetDestructionCallback(base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

void FakeDataRetriever::BuildDefaultDataToRetrieve(const GURL& url,
                                                   const GURL& scope) {
  SetEmptyRendererWebAppInstallInfo();

  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = url;
  manifest->id = GenerateManifestIdFromStartUrlOnly(manifest->start_url);
  manifest->scope = scope;
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"Manifest Name";

  SetManifest(std::move(manifest),
              /*error_code=*/webapps::InstallableStatusCode::NO_ERROR_DETECTED);

  SetIcons(IconsMap{});
}

void FakeDataRetriever::ScheduleCompletionCallback() {
  // If |this| DataRetriever destroyed, the completion callback gets cancelled.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDataRetriever::CallCompletionCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeDataRetriever::CallCompletionCallback() {
  std::move(completion_callback_).Run();
}

}  // namespace web_app
