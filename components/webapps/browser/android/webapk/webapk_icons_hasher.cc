// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapp_icon.h"
#include "content/public/browser/web_contents.h"

namespace webapps {
namespace {

// The default number of milliseconds to wait for the icon download to complete.
const int kDownloadTimeoutInMilliseconds = 60000;

}  // anonymous namespace

WebApkIconsHasher::WebApkIconsHasher() = default;
WebApkIconsHasher::~WebApkIconsHasher() = default;

WebApkIconsHasher::PassKey WebApkIconsHasher::PassKeyForTesting() {
  return PassKey();
}

void WebApkIconsHasher::DownloadAndComputeMurmur2Hash(
    network::mojom::URLLoaderFactory* url_loader_factory,
    base::WeakPtr<content::WebContents> web_contents,
    const url::Origin& request_initiator,
    const ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon_bitmap,
    Murmur2HashMultipleCallback callback) {
  webapk_icons_ = shortcut_info.GetWebApkIcons();

  auto barrier_closure = base::BarrierClosure(
      webapk_icons_.size(),
      base::BindOnce(&WebApkIconsHasher::OnAllMurmur2Hashes,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     shortcut_info.best_primary_icon_url,
                     shortcut_info.is_primary_icon_maskable,
                     primary_icon_bitmap));

  for (auto& [icon_url, webapk_icon] : webapk_icons_) {
    hashers_.emplace_back(std::make_unique<WebApkSingleIconHasher>(
        PassKey(), url_loader_factory, web_contents, request_initiator,
        kDownloadTimeoutInMilliseconds, webapk_icon.get(), barrier_closure));
  }
}

void WebApkIconsHasher::OnAllMurmur2Hashes(
    WebApkIconsHasher::Murmur2HashMultipleCallback callback,
    const GURL& primary_icon_url,
    bool primary_icon_maskable,
    const SkBitmap& primary_icon_bitmap) {
  auto primary_icon_it = webapk_icons_.find(primary_icon_url);
  if (primary_icon_it == webapk_icons_.end() ||
      !primary_icon_it->second->has_unsafe_data()) {
    webapk_icons_[primary_icon_url] = std::make_unique<WebappIcon>(
        primary_icon_url, primary_icon_maskable, webapk::Image::PRIMARY_ICON);
    WebApkSingleIconHasher::SetIconDataAndHashFromSkBitmap(
        webapk_icons_[primary_icon_url].get(), primary_icon_bitmap);
  }
  hashers_.clear();
  std::move(callback).Run(std::move(webapk_icons_));
}

}  // namespace webapps
