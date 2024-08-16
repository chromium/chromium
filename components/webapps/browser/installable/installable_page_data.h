// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PAGE_DATA_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PAGE_DATA_H_

#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

class InstallablePageData {
 public:
  InstallablePageData();
  ~InstallablePageData();

  InstallablePageData(const InstallablePageData&) = delete;
  InstallablePageData& operator=(const InstallablePageData&) = delete;

  InstallablePageData(InstallablePageData&& other) noexcept;
  InstallablePageData& operator=(InstallablePageData&& other);

  void Reset();

  void OnManifestFetched(
      blink::mojom::ManifestPtr manifest,
      GURL manifest_url,
      InstallableStatusCode error = InstallableStatusCode::NO_ERROR_DETECTED);
  void OnPageMetadataFetched(mojom::WebPageMetadataPtr web_page_metadata);
  void OnPrimaryIconFetched(const GURL& icon_url,
                            const IconPurpose purpose,
                            const SkBitmap& bitmap);
  void OnPrimaryIconFetchedError(InstallableStatusCode code);
  void OnScreenshotsDownloaded(std::vector<Screenshot> screenshots);

  const blink::mojom::Manifest& GetManifest() const;
  const mojom::WebPageMetadata& WebPageMetadata() const;

  const GURL& manifest_url() const { return manifest_->url; }
  InstallableStatusCode manifest_error() const { return manifest_->error; }
  bool manifest_fetched() const { return manifest_->fetched; }
  bool web_page_metadata_fetched() const { return web_page_metadata_->fetched; }
  const SkBitmap* primary_icon() const { return primary_icon_->icon.get(); }
  IconPurpose primary_icon_purpose() const { return primary_icon_->purpose; }
  const GURL& primary_icon_url() const { return primary_icon_->url; }
  InstallableStatusCode icon_error() const { return primary_icon_->error; }
  bool primary_icon_fetched() const { return primary_icon_->fetched; }
  const std::vector<Screenshot>& screenshots() const { return screenshots_; }
  bool is_screenshots_fetch_complete() const {
    return is_screenshots_fetch_complete_;
  }

 private:
  friend class InstallableEvaluatorUnitTest;

  struct ManifestProperty {
    ManifestProperty();
    ~ManifestProperty();

    InstallableStatusCode error = InstallableStatusCode::NO_ERROR_DETECTED;
    //  This can be empty if the page doesn't have a manifest url.
    GURL url;
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    bool fetched = false;
  };

  struct WebPageMetadataProperty {
    WebPageMetadataProperty();
    ~WebPageMetadataProperty();

    mojom::WebPageMetadataPtr metadata = mojom::WebPageMetadata::New();
    bool fetched = false;
  };

  struct IconProperty {
    IconProperty();

    IconProperty(const IconProperty&) = delete;
    IconProperty& operator=(const IconProperty&) = delete;

    IconProperty(IconProperty&& other) noexcept;
    IconProperty& operator=(IconProperty&& other);

    ~IconProperty();

    InstallableStatusCode error = InstallableStatusCode::NO_ERROR_DETECTED;
    IconPurpose purpose = IconPurpose::ANY;
    GURL url;
    std::unique_ptr<SkBitmap> icon;
    bool fetched = false;
  };

  std::unique_ptr<ManifestProperty> manifest_;
  std::unique_ptr<WebPageMetadataProperty> web_page_metadata_;
  std::unique_ptr<IconProperty> primary_icon_;
  std::vector<Screenshot> screenshots_;
  bool is_screenshots_fetch_complete_ = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PAGE_DATA_H_
