// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_FETCHER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_icon_fetcher.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

using FetcherCallback = base::OnceCallback<void(InstallableStatusCode)>;

// This class is responsible for fetching the resources required to install a
// site and store them into InstallablePageData. It'll return the result if the
// result is already cached in the InstallablePageData.
class InstallableDataFetcher {
 public:
  InstallableDataFetcher(content::WebContents* web_contents,
                         InstallablePageData& data);

  ~InstallableDataFetcher();

  void FetchManifest(FetcherCallback finish_callback);
  void FetchWebPageMetadata(FetcherCallback finish_callback);
  void CheckAndFetchBestPrimaryIcon(FetcherCallback finish_callback,
                                    bool prefer_maskable,
                                    bool fetch_favicon);
  void CheckAndFetchScreenshots(FetcherCallback finish_callback);

 private:
  void OnDidGetManifest(FetcherCallback finish_callback,
                        blink::mojom::ManifestRequestResult result,
                        const GURL& manifest_url,
                        blink::mojom::ManifestPtr manifest);

  void OnDidGetWebPageMetadata(
      mojo::AssociatedRemote<mojom::WebPageMetadataAgent> metadata_agent,
      FetcherCallback finish_callback,
      mojom::WebPageMetadataPtr web_page_metadata);

  void OnScreenshotFetched(GURL screenshot_url, const SkBitmap& bitmap);

  content::WebContents* web_contents() { return web_contents_.get(); }

  base::WeakPtr<content::WebContents> web_contents_;

  const raw_ref<InstallablePageData> page_data_;

  std::unique_ptr<InstallableIconFetcher> icon_fetcher_;

  // A map of screenshots downloaded. Used temporarily until images are moved to
  // the screenshots_ member.
  std::map<GURL, SkBitmap> downloaded_screenshots_;
  // The number of screenshots currently being downloaded.
  int screenshots_downloading_ = 0;
  FetcherCallback screenshot_complete_;

  base::WeakPtrFactory<InstallableDataFetcher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_FETCHER_H_
