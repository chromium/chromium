// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_data_retriever.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
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
    CheckInstallabilityCallback callback,
    std::optional<webapps::InstallableParams> params) {
  completion_callback_ =
      base::BindOnce(std::move(callback), manifest_.Clone(),
                     /*valid_manifest_for_web_app=*/true, error_code_);
  ScheduleCompletionCallback();
}

void FakeDataRetriever::GetIcons(content::WebContents* web_contents,
                                 const IconUrlSizeSet& icon_urls,
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

void FakeDataRetriever::SetWebPageMetadata(
    const GURL& last_committed_url,
    const std::u16string& title,
    std::optional<webapps::mojom::WebPageMetadata> opt_metadata) {
  CHECK(last_committed_url.is_valid());
  GURL fallback_start_url = last_committed_url;
  std::u16string fallback_title = title;
  if (fallback_title.empty()) {
    fallback_title = base::UTF8ToUTF16(fallback_start_url.spec());
  }
  web_app_info_ = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(fallback_start_url),
      fallback_start_url);
  if (opt_metadata) {
    WebAppDataRetriever::PopulateWebAppInfoFromMetadata(web_app_info_.get(),
                                                        opt_metadata.value());
  }
}

void FakeDataRetriever::SetManifest(blink::mojom::ManifestPtr manifest,
                                    webapps::InstallableStatusCode error_code) {
  manifest_ = std::move(manifest);
  error_code_ = error_code;
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
  SetWebPageMetadata(url, u"Page Title",
                     /*opt_metadata=*/std::nullopt);

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
