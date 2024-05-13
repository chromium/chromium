// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICONS_HASHER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICONS_HASHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/webapps/browser/android/webapk/webapk_single_icon_hasher.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::mojom {
class URLLoaderFactory;
}

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

struct ShortcutInfo;
class WebappIcon;

// Downloads WebAPK icons and takes a Murmur2 hashes of the downloaded images.
class WebApkIconsHasher {
 public:
  using Murmur2HashMultipleCallback =
      base::OnceCallback<void(std::map<GURL, std::unique_ptr<WebappIcon>>)>;

  using PassKey = base::PassKey<WebApkIconsHasher>;
  static PassKey PassKeyForTesting();

  WebApkIconsHasher();
  ~WebApkIconsHasher();
  WebApkIconsHasher(const WebApkIconsHasher&) = delete;
  WebApkIconsHasher& operator=(const WebApkIconsHasher&) = delete;

  // Creates a self-owned WebApkIconHasher instance. The instance downloads all
  // the |icon_urls| and calls |callback| with the Murmur2 hash of the
  // downloaded images. The hash is taken over the raw image bytes (no image
  // encoding/decoding beforehand). |callback| is called with a std::nullopt if
  // any image cannot not be downloaded in time (e.g. 404 HTTP error code).
  void DownloadAndComputeMurmur2Hash(
      network::mojom::URLLoaderFactory* url_loader_factory,
      base::WeakPtr<content::WebContents> web_contents,
      const url::Origin& request_initiator,
      const ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon_bitmap,
      Murmur2HashMultipleCallback callback);

 private:
  void OnAllMurmur2Hashes(Murmur2HashMultipleCallback callback,
                          const GURL& primary_icon_url,
                          bool primary_icon_maskable,
                          const SkBitmap& primary_icon_bitmap);

  std::map<GURL, std::unique_ptr<WebappIcon>> webapk_icons_;
  std::vector<std::unique_ptr<WebApkSingleIconHasher>> hashers_;

  base::WeakPtrFactory<WebApkIconsHasher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICONS_HASHER_H_
