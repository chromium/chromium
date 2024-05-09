// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_single_icon_hasher.h"
#include "components/webapps/browser/android/webapp_icon.h"
#include "content/public/browser/web_contents.h"

namespace webapps {
namespace {

// The default number of milliseconds to wait for the icon download to complete.
const int kDownloadTimeoutInMilliseconds = 60000;

}  // anonymous namespace

WebApkIconsHasher::WebApkIconsHasher() = default;
WebApkIconsHasher::~WebApkIconsHasher() = default;

void WebApkIconsHasher::DownloadAndComputeMurmur2Hash(
    network::mojom::URLLoaderFactory* url_loader_factory,
    base::WeakPtr<content::WebContents> web_contents,
    const url::Origin& request_initiator,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    Murmur2HashMultipleCallback callback) {
  webapk_icons_ = std::move(icons);

  auto barrier_closure = base::BarrierClosure(
      webapk_icons_.size(),
      base::BindOnce(&WebApkIconsHasher::OnAllMurmur2Hashes,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  for (auto& [icon_url, webapk_icon] : webapk_icons_) {
    // |hashes| is owned by |barrier_closure|.
    WebApkSingleIconHasher::DownloadAndComputeMurmur2HashWithTimeout(
        url_loader_factory, web_contents, request_initiator,
        kDownloadTimeoutInMilliseconds, webapk_icon.get(), barrier_closure);
  }
}

void WebApkIconsHasher::OnAllMurmur2Hashes(
    WebApkIconsHasher::Murmur2HashMultipleCallback callback) {
  for (auto& [icon_url, webapk_icon] : webapk_icons_) {
    // Return an empty map if downloading primary icon is unsuccessful.
    if (webapk_icon->usages().contains(webapk::Image::PRIMARY_ICON) &&
        !webapk_icon->has_unsafe_data()) {
      std::move(callback).Run(std::map<GURL, std::unique_ptr<WebappIcon>>());
      return;
    }
  }
  std::move(callback).Run(std::move(webapk_icons_));
}

}  // namespace webapps
