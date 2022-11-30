// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

// Downloads an icon and takes a Murmur2 hash of the downloaded image.
class WebApkIconHasher {
 public:
  // Result struct for holding the downloaded icon data and its hash.
  struct Icon {
    // The result of fetching the |icon|. This is untrusted data from the web
    // and should not be processed or decoded by the browser process.
    std::string unsafe_data;

    // The murmur2 hash of |unsafe_data|.
    std::string hash;
  };

  using Murmur2HashCallback = base::OnceCallback<void(Icon)>;
  using Murmur2HashMultipleCallback =
      base::OnceCallback<void(absl::optional<std::map<std::string, Icon>>)>;

  WebApkIconHasher(const WebApkIconHasher&) = delete;
  WebApkIconHasher& operator=(const WebApkIconHasher&) = delete;

  // Creates a self-owned WebApkIconHasher instance. The instance downloads all
  // the |icon_urls| and calls |callback| with the Murmur2 hash of the
  // downloaded images. The hash is taken over the raw image bytes (no image
  // encoding/decoding beforehand). |callback| is called with a absl::nullopt if
  // any image cannot not be downloaded in time (e.g. 404 HTTP error code).
  static void DownloadAndComputeMurmur2Hash(
      network::mojom::URLLoaderFactory* url_loader_factory,
      base::WeakPtr<content::WebContents> web_contents,
      const url::Origin& request_initiator,
      const std::set<GURL>& icon_urls,
      Murmur2HashMultipleCallback callback);

  static void DownloadAndComputeMurmur2HashWithTimeout(
      network::mojom::URLLoaderFactory* url_loader_factory,
      base::WeakPtr<content::WebContents> web_contents,
      const url::Origin& request_initiator,
      const GURL& icon_url,
      int timeout_ms,
      Murmur2HashCallback callback);

 private:
  WebApkIconHasher(network::mojom::URLLoaderFactory* url_loader_factory,
                   base::WeakPtr<content::WebContents> web_contents,
                   const url::Origin& request_initiator,
                   const GURL& icon_url,
                   int timeout_ms,
                   Murmur2HashCallback callback);
  ~WebApkIconHasher();

  void OnSimpleLoaderComplete(base::WeakPtr<content::WebContents> web_contents,
                              int timeout_ms,
                              std::unique_ptr<std::string> response_body);

  void OnImageDownloaded(std::unique_ptr<std::string> response_body,
                         int id,
                         int http_status_code,
                         const GURL& url,
                         const std::vector<SkBitmap>& bitmaps,
                         const std::vector<gfx::Size>& sizes);

  // Called if downloading the icon takes too long.
  void OnDownloadTimedOut();

  // Calls |callback_| with |icon_murmur2_hash|. Also deletes the instance.
  void RunCallback(Icon icon_murmur2_hash);

  // Called with the image hash.
  Murmur2HashCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Fails WebApkIconHasher if the download takes too long.
  base::OneShotTimer download_timeout_timer_;

  base::WeakPtrFactory<WebApkIconHasher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
