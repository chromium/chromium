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

class WebappIcon;

// Downloads WebAPK icons and takes a Murmur2 hashes of the downloaded images.
class WebApkIconsHasher {
 public:
  // Result struct for holding the downloaded icon data and its hash.
  struct Icon {
    // The result of fetching the |icon|. This is untrusted data from the web
    // and should not be processed or decoded by the browser process.
    std::string unsafe_data;

    // The murmur2 hash of |unsafe_data|.
    std::string hash;
  };
  using Murmur2HashMultipleCallback =
      base::OnceCallback<void(std::map<GURL, std::unique_ptr<WebappIcon>>)>;
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
      std::map<GURL, std::unique_ptr<WebappIcon>> icons,
      Murmur2HashMultipleCallback callback);

 private:
  void OnAllMurmur2Hashes(Murmur2HashMultipleCallback callback);

  std::map<GURL, std::unique_ptr<WebappIcon>> webapk_icons_;

  base::WeakPtrFactory<WebApkIconsHasher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICONS_HASHER_H_
