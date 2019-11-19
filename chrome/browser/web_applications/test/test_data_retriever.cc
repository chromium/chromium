// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_data_retriever.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

TestDataRetriever::TestDataRetriever() = default;

TestDataRetriever::~TestDataRetriever() {
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

void TestDataRetriever::GetWebApplicationInfo(
    content::WebContents* web_contents,
    GetWebApplicationInfoCallback callback) {
  DCHECK(web_contents);

  completion_callback_ =
      base::BindOnce(std::move(callback), std::move(web_app_info_));
  ScheduleCompletionCallback();
}

void TestDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    bool bypass_service_worker_check,
    CheckInstallabilityCallback callback) {
  DCHECK(manifest_);

  completion_callback_ =
      base::BindOnce(std::move(callback), *manifest_,
                     /*valid_manifest_for_web_app=*/true, is_installable_);
  ScheduleCompletionCallback();
}

void TestDataRetriever::GetIcons(content::WebContents* web_contents,
                                 const std::vector<GURL>& icon_urls,
                                 bool skip_page_favicons,
                                 WebAppIconDownloader::Histogram histogram,
                                 GetIconsCallback callback) {
  if (get_icons_delegate_) {
    icons_map_ =
        get_icons_delegate_.Run(web_contents, icon_urls, skip_page_favicons);
  }

  completion_callback_ =
      base::BindOnce(std::move(callback), std::move(icons_map_));
  ScheduleCompletionCallback();

  icons_map_.clear();
}

void TestDataRetriever::SetRendererWebApplicationInfo(
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  web_app_info_ = std::move(web_app_info);
}

void TestDataRetriever::SetEmptyRendererWebApplicationInfo() {
  SetRendererWebApplicationInfo(std::make_unique<WebApplicationInfo>());
}

void TestDataRetriever::SetManifest(std::unique_ptr<blink::Manifest> manifest,
                                    bool is_installable) {
  manifest_ = std::move(manifest);
  is_installable_ = is_installable;
}

void TestDataRetriever::SetIcons(IconsMap icons_map) {
  DCHECK(!get_icons_delegate_);
  icons_map_ = std::move(icons_map);
}

void TestDataRetriever::SetGetIconsDelegate(
    GetIconsDelegate get_icons_delegate) {
  DCHECK(icons_map_.empty());
  get_icons_delegate_ = std::move(get_icons_delegate);
}

void TestDataRetriever::SetDestructionCallback(base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

void TestDataRetriever::BuildDefaultDataToRetrieve(const GURL& url,
                                                   const GURL& scope) {
  SetEmptyRendererWebApplicationInfo();

  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = url;
  manifest->scope = scope;

  SetManifest(std::move(manifest), /*is_installable=*/true);

  SetIcons(IconsMap{});
}

void TestDataRetriever::ScheduleCompletionCallback() {
  // If |this| DataRetriever destroyed, the completion callback gets cancelled.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestDataRetriever::CallCompletionCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void TestDataRetriever::CallCompletionCallback() {
  std::move(completion_callback_).Run();
}

}  // namespace web_app
