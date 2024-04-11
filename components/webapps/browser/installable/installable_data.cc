// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_data.h"

#include <utility>

#include "base/containers/flat_set.h"
#include "components/webapps/browser/installable/installable_logging.h"

namespace webapps {

Screenshot::Screenshot(SkBitmap image, std::optional<std::u16string> label)
    : image(std::move(image)), label(label) {}

Screenshot::Screenshot(const Screenshot& screenshot) = default;
Screenshot& Screenshot::operator=(const Screenshot& screenshot) = default;

Screenshot::~Screenshot() = default;

InstallableData::InstallableData(std::vector<InstallableStatusCode> errors,
                                 const GURL& manifest_url,
                                 const blink::mojom::Manifest& manifest,
                                 const mojom::WebPageMetadata& metadata,
                                 const GURL& primary_icon_url,
                                 const SkBitmap* primary_icon,
                                 bool has_maskable_primary_icon,
                                 const std::vector<Screenshot>& screenshots,
                                 bool installable_check_passed)
    : errors(std::move(errors)),
      manifest_url(manifest_url),
      manifest(manifest),
      web_page_metadata(metadata),
      primary_icon_url(primary_icon_url),
      primary_icon(primary_icon),
      has_maskable_primary_icon(has_maskable_primary_icon),
      screenshots(screenshots),
      installable_check_passed(installable_check_passed) {}

InstallableData::~InstallableData() = default;

InstallableStatusCode InstallableData::GetFirstError() const {
  if (errors.empty()) {
    return InstallableStatusCode::NO_ERROR_DETECTED;
  }
  return errors[0];
}

}  // namespace webapps
