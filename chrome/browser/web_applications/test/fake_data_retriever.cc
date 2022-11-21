// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_data_retriever.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
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
                     /*valid_manifest_for_web_app=*/true, is_installable_);
  ScheduleCompletionCallback();
}

void FakeDataRetriever::GetIcons(content::WebContents* web_contents,
                                 base::flat_set<GURL> icon_urls,
                                 bool skip_page_favicons,
                                 GetIconsCallback callback) {
  if (get_icons_delegate_) {
    icons_map_ =
        get_icons_delegate_.Run(web_contents, icon_urls, skip_page_favicons);
  }

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
                                    bool is_installable,
                                    GURL manifest_url) {
  manifest_ = std::move(manifest);
  is_installable_ = is_installable;
  manifest_url_ = std::move(manifest_url);
}

void FakeDataRetriever::SetIcons(IconsMap icons_map) {
  DCHECK(!get_icons_delegate_);
  icons_map_ = std::move(icons_map);
}

void FakeDataRetriever::SetGetIconsDelegate(
    GetIconsDelegate get_icons_delegate) {
  DCHECK(icons_map_.empty());
  get_icons_delegate_ = std::move(get_icons_delegate);
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
  manifest->scope = scope;
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"Manifest Name";

  SetManifest(std::move(manifest), /*is_installable=*/true);

  SetIcons(IconsMap{});
}

void FakeDataRetriever::ScheduleCompletionCallback() {
  // If |this| DataRetriever destroyed, the completion callback gets cancelled.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDataRetriever::CallCompletionCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeDataRetriever::CallCompletionCallback() {
  std::move(completion_callback_).Run();
}

}  // namespace web_app
