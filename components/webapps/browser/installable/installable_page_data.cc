// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_page_data.h"
namespace webapps {

namespace {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

}

InstallablePageData::ManifestProperty::ManifestProperty() = default;
InstallablePageData::ManifestProperty::~ManifestProperty() = default;

InstallablePageData::WebPageMetadataProperty::WebPageMetadataProperty() =
    default;
InstallablePageData::WebPageMetadataProperty::~WebPageMetadataProperty() =
    default;

InstallablePageData::IconProperty::IconProperty() = default;
InstallablePageData::IconProperty::~IconProperty() = default;

InstallablePageData::IconProperty::IconProperty(IconProperty&& other) noexcept =
    default;
InstallablePageData::IconProperty& InstallablePageData::IconProperty::operator=(
    InstallablePageData::IconProperty&& other) = default;

InstallablePageData::InstallablePageData()
    : manifest_(std::make_unique<ManifestProperty>()),
      web_page_metadata_(std::make_unique<WebPageMetadataProperty>()),
      primary_icon_(std::make_unique<IconProperty>()) {}

InstallablePageData::~InstallablePageData() = default;

void InstallablePageData::Reset() {
  manifest_ = std::make_unique<ManifestProperty>();
  web_page_metadata_ = std::make_unique<WebPageMetadataProperty>();
  primary_icon_ = std::make_unique<IconProperty>();
  screenshots_.clear();
  is_screenshots_fetch_complete_ = false;
}

void InstallablePageData::OnManifestFetched(blink::mojom::ManifestPtr manifest,
                                            GURL manifest_url,
                                            InstallableStatusCode error_code) {
  CHECK(!manifest_->fetched);
  manifest_->manifest = std::move(manifest);
  manifest_->url = manifest_url;
  manifest_->error = error_code;
  manifest_->fetched = true;
}

void InstallablePageData::OnPageMetadataFetched(
    mojom::WebPageMetadataPtr web_page_metadata) {
  CHECK(!web_page_metadata_->fetched);
  web_page_metadata_->metadata = std::move(web_page_metadata);
  web_page_metadata_->fetched = true;
}

void InstallablePageData::OnPrimaryIconFetched(const GURL& icon_url,
                                               const IconPurpose purpose,
                                               const SkBitmap& bitmap) {
  primary_icon_->fetched = true;
  primary_icon_->url = icon_url;
  primary_icon_->icon = std::make_unique<SkBitmap>(bitmap);
  primary_icon_->purpose = purpose;
  primary_icon_->error = InstallableStatusCode::NO_ERROR_DETECTED;
}

void InstallablePageData::OnPrimaryIconFetchedError(
    InstallableStatusCode code) {
  primary_icon_->fetched = true;
  primary_icon_->error = code;
}

void InstallablePageData::OnScreenshotsDownloaded(
    std::vector<Screenshot> screenshots) {
  CHECK(!is_screenshots_fetch_complete_);
  screenshots_ = std::move(screenshots);
  is_screenshots_fetch_complete_ = true;
}

const blink::mojom::Manifest& InstallablePageData::GetManifest() const {
  DCHECK(manifest_->manifest);
  return *manifest_->manifest;
}

const mojom::WebPageMetadata& InstallablePageData::WebPageMetadata() const {
  DCHECK(web_page_metadata_->metadata);
  return *web_page_metadata_->metadata;
}

}  // namespace webapps
