// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/web_app_banner_data.h"

#include <string>

#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

WebAppBannerData::WebAppBannerData(ManifestId manifest_id,
                                   blink::mojom::ManifestPtr manifest_ptr,
                                   mojom::WebPageMetadataPtr metadata,
                                   GURL manifest_url)
    : manifest_id(std::move(manifest_id)),
      manifest_url(std::move(manifest_url)),
      manifest_ptr(std::move(manifest_ptr)),
      web_page_metadata_ptr(std::move(metadata)) {
  CHECK(this->manifest_id.is_valid());
  CHECK(this->manifest_ptr);
  CHECK(this->web_page_metadata_ptr);
}
WebAppBannerData::WebAppBannerData(ManifestId manifest_id,
                                   const blink::mojom::Manifest& manifest,
                                   const mojom::WebPageMetadata& metadata,
                                   GURL manifest_url)
    : manifest_id(std::move(manifest_id)),
      manifest_url(std::move(manifest_url)),
      manifest_ptr(manifest.Clone()),
      web_page_metadata_ptr(metadata.Clone()) {
  CHECK(this->manifest_id.is_valid());
}
WebAppBannerData::WebAppBannerData(const WebAppBannerData& other)
    : manifest_id(other.manifest_id),
      manifest_url(other.manifest_url),
      primary_icon_url(other.primary_icon_url),
      primary_icon(other.primary_icon),
      has_maskable_primary_icon(other.has_maskable_primary_icon),
      screenshots(other.screenshots),
      manifest_ptr(other.manifest_ptr.Clone()),
      web_page_metadata_ptr(other.web_page_metadata_ptr->Clone()) {}
WebAppBannerData::~WebAppBannerData() = default;

const std::u16string& WebAppBannerData::GetAppName() const {
  if (manifest().name.has_value()) {
    return manifest().name.value();
  }
  if (!web_page_metadata().application_name.empty()) {
    return web_page_metadata().application_name;
  }
  return web_page_metadata().title;
}

const blink::mojom::Manifest& WebAppBannerData::manifest() const {
  return *manifest_ptr;
}

const mojom::WebPageMetadata& WebAppBannerData::web_page_metadata() const {
  return *web_page_metadata_ptr;
}

}  // namespace webapps
