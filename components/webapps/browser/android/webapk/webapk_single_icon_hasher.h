// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_SINGLE_ICON_HASHER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_SINGLE_ICON_HASHER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "components/webapps/browser/android/webapp_icon.h"
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

class WebApkIconsHasher;

// Downloads an icon and takes a Murmur2 hash of the downloaded image.
class WebApkSingleIconHasher {
 public:
  WebApkSingleIconHasher(base::PassKey<WebApkIconsHasher> pass_key,
                         network::mojom::URLLoaderFactory* url_loader_factory,
                         base::WeakPtr<content::WebContents> web_contents,
                         const url::Origin& request_initiator,
                         int timeout_ms,
                         WebappIcon* webapk_icon,
                         base::OnceClosure callback);
  ~WebApkSingleIconHasher();

  WebApkSingleIconHasher(const WebApkSingleIconHasher&) = delete;
  WebApkSingleIconHasher& operator=(const WebApkSingleIconHasher&) = delete;

  static void SetIconDataAndHashFromSkBitmap(
      WebappIcon* icon,
      const SkBitmap& bitmap,
      std::unique_ptr<std::string> response_body = nullptr);

 private:
  void OnSimpleLoaderComplete(base::WeakPtr<content::WebContents> web_contents,
                              int ideal_icon_size,
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
  void RunCallbackAndFinish();

  raw_ptr<WebappIcon> icon_;

  // Called with the image hash.
  base::OnceClosure callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Fails WebApkSingleIconHasher if the download takes too long.
  base::OneShotTimer download_timeout_timer_;


  base::WeakPtrFactory<WebApkSingleIconHasher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_SINGLE_ICON_HASHER_H_
