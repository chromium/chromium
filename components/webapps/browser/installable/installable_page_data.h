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

struct InstallablePageData {
  struct ManifestProperty {
    ManifestProperty();
    ~ManifestProperty();

    InstallableStatusCode error = NO_ERROR_DETECTED;
    GURL url;
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    bool fetched = false;
  };

  struct WebPageMetadataProperty {
    WebPageMetadataProperty();
    ~WebPageMetadataProperty();

    InstallableStatusCode error = NO_ERROR_DETECTED;
    mojom::WebPageMetadataPtr metadata = mojom::WebPageMetadata::New();
    bool fetched = false;
  };

  struct ServiceWorkerProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    bool has_worker = false;
    bool is_waiting = false;
    bool fetched = false;
  };

  struct IconProperty {
    IconProperty();

    IconProperty(const IconProperty&) = delete;
    IconProperty& operator=(const IconProperty&) = delete;

    IconProperty(IconProperty&& other) noexcept;
    IconProperty& operator=(IconProperty&& other);

    ~IconProperty();

    InstallableStatusCode error = NO_ERROR_DETECTED;
    IconPurpose purpose = blink::mojom::ManifestImageResource_Purpose::ANY;
    GURL url;
    std::unique_ptr<SkBitmap> icon;
    bool fetched = false;
  };

  InstallablePageData();
  ~InstallablePageData();

  InstallablePageData(const InstallablePageData&) = delete;
  InstallablePageData& operator=(const InstallablePageData&) = delete;

  InstallablePageData(InstallablePageData&& other) noexcept;
  InstallablePageData& operator=(InstallablePageData&& other);

  void Reset();

  const blink::mojom::Manifest& GetManifest() const;

  std::unique_ptr<ManifestProperty> manifest;
  std::unique_ptr<WebPageMetadataProperty> web_page_metadata;
  std::unique_ptr<ServiceWorkerProperty> worker;
  std::unique_ptr<IconProperty> primary_icon;
  std::vector<Screenshot> screenshots;
  bool is_screenshots_fetch_complete = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_PAGE_DATA_H_
