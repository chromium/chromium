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
    : manifest(std::make_unique<ManifestProperty>()),
      web_page_metadata(std::make_unique<WebPageMetadataProperty>()),
      worker(std::make_unique<ServiceWorkerProperty>()),
      primary_icon(std::make_unique<IconProperty>()) {}

InstallablePageData::~InstallablePageData() = default;

void InstallablePageData::Reset() {
  manifest = std::make_unique<ManifestProperty>();
  web_page_metadata = std::make_unique<WebPageMetadataProperty>();
  worker = std::make_unique<ServiceWorkerProperty>();
  primary_icon = std::make_unique<IconProperty>();
  is_screenshots_fetch_complete = false;
}

const blink::mojom::Manifest& InstallablePageData::GetManifest() const {
  DCHECK(manifest->manifest);
  return *manifest->manifest;
}

const mojom::WebPageMetadata& InstallablePageData::WebPageMetadata() const {
  DCHECK(web_page_metadata->metadata);
  return *web_page_metadata->metadata;
}

}  // namespace webapps
