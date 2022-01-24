// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_data.h"

#include <utility>

namespace webapps {

InstallableData::InstallableData(std::vector<InstallableStatusCode> errors,
                                 const GURL& manifest_url,
                                 const blink::mojom::Manifest& manifest,
                                 const GURL& primary_icon_url,
                                 const SkBitmap* primary_icon,
                                 bool has_maskable_primary_icon,
                                 const GURL& splash_icon_url,
                                 const SkBitmap* splash_icon,
                                 bool has_maskable_splash_icon,
                                 const std::vector<SkBitmap>& screenshots,
                                 bool valid_manifest,
                                 bool has_worker)
    : errors(std::move(errors)),
      manifest_url(manifest_url),
      manifest(manifest),
      primary_icon_url(primary_icon_url),
      primary_icon(primary_icon),
      has_maskable_primary_icon(has_maskable_primary_icon),
      splash_icon_url(splash_icon_url),
      splash_icon(splash_icon),
      has_maskable_splash_icon(has_maskable_splash_icon),
      screenshots(screenshots),
      valid_manifest(valid_manifest),
      has_worker(has_worker) {}

InstallableData::~InstallableData() = default;

bool InstallableData::NoBlockingErrors() const {
  return errors.empty() ||
         (errors.size() == 1 && errors[0] == WARN_NOT_OFFLINE_CAPABLE);
}

}  // namespace webapps
