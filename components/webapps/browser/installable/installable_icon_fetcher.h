// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_ICON_FETCHER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_ICON_FETCHER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/android_buildflags.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace favicon_base {
struct LargeIconResult;
}

namespace webapps {

namespace test {
extern int g_minimum_favicon_size_for_testing;
}  // namespace test

// This class is responsible for fetching the primary icon for installing a site
// When done, it'll store the result in InstallablePageData and run the
// finish_callback.
class InstallableIconFetcher {
 public:
  InstallableIconFetcher(
      content::WebContents* web_contents,
      InstallablePageData& data,
      const std::vector<blink::Manifest::ImageResource>& manifest_icons,
      bool prefer_maskable,
      bool fetch_favicon,
      base::OnceCallback<void(InstallableStatusCode)> finish_callback);
  ~InstallableIconFetcher();

  InstallableIconFetcher(const InstallableIconFetcher&) = delete;
  InstallableIconFetcher& operator=(const InstallableIconFetcher&) = delete;

 private:
  void TryFetchingNextIcon();
  void OnManifestIconFetched(
      const GURL& icon_url,
      const blink::mojom::ManifestImageResource_Purpose purpose,
      const SkBitmap& bitmap);

  void FetchFavicon();
  void OnFaviconFetched(const favicon_base::LargeIconResult& result);

  void OnIconFetched(const GURL& icon_url,
                     const blink::mojom::ManifestImageResource_Purpose purpose,
                     const SkBitmap& bitmap);

  // Gives a chance for desktop android to generate an icon instead of
  // ending with an error. Other platforms end immediately with an error.
  void MaybeEndWithError(InstallableStatusCode code);

  // Ends the fetch with an error.
  void EndWithError(InstallableStatusCode code);

#if BUILDFLAG(IS_DESKTOP_ANDROID)
  void OnHomeScreenIconGenerated(const GURL& page_url, const SkBitmap& bitmap);
#endif

  base::WeakPtr<content::WebContents> web_contents_;

  const raw_ref<InstallablePageData> page_data_;

  const raw_ref<const std::vector<blink::Manifest::ImageResource>>
      manifest_icons_;
  const bool prefer_maskable_;
  const bool fetch_favicon_;
  base::OnceCallback<void(InstallableStatusCode)> finish_callback_;

  std::vector<IconPurpose> downloading_icons_type_;
  base::CancelableTaskTracker favicon_task_tracker_;

  base::WeakPtrFactory<InstallableIconFetcher> weak_ptr_factory_{this};
};
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_ICON_FETCHER_H_
