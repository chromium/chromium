// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_BANNERS_WEB_APP_BANNER_DATA_H_
#define COMPONENTS_WEBAPPS_BROWSER_BANNERS_WEB_APP_BANNER_DATA_H_

#include <string>

#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

// Holds information about a web app being considered for an installation
// banner / onbeforeinstallprompt event.
struct WebAppBannerData {
  WebAppBannerData() = delete;
  WebAppBannerData(ManifestId manifest_id,
                   blink::mojom::ManifestPtr manifest,
                   mojom::WebPageMetadataPtr metadata,
                   GURL manifest_url);
  WebAppBannerData(ManifestId manifest_id,
                   const blink::mojom::Manifest& manifest,
                   const mojom::WebPageMetadata& metadata,
                   GURL manifest_url);
  WebAppBannerData(const WebAppBannerData&);
  ~WebAppBannerData();

  const std::u16string& GetAppName() const;

  const blink::mojom::Manifest& manifest() const;

  const mojom::WebPageMetadata& web_page_metadata() const;

  // The ManifestId for this webpage. This should be used instead of the
  // manifest's manifest_id.
  const ManifestId manifest_id;

  // The URL of the manifest.
  const GURL manifest_url;

  // The URL of the primary icon.
  GURL primary_icon_url;

  // The primary icon object.
  SkBitmap primary_icon;

  // Whether or not the primary icon is maskable.
  bool has_maskable_primary_icon = false;

  // The screenshots to show in the install UI.
  std::vector<Screenshot> screenshots;

 private:
  // The manifest object. This is never null, it will instead be an empty
  // manifest so callers don't have to worry about null checks.
  const blink::mojom::ManifestPtr manifest_ptr;

  // The web page metadata object. This is never null, it will instead be
  // empty so callers don't have to worry about null checks.
  const mojom::WebPageMetadataPtr web_page_metadata_ptr;
};

}  // namespace webapps
#endif  // COMPONENTS_WEBAPPS_BROWSER_BANNERS_WEB_APP_BANNER_DATA_H_
