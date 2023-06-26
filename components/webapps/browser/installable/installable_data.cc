// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_data.h"

#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_logging.h"

namespace webapps {

Screenshot::Screenshot(SkBitmap image, absl::optional<std::u16string> label)
    : image(std::move(image)), label(label) {}

Screenshot::Screenshot(const Screenshot& screenshot) = default;
Screenshot& Screenshot::operator=(const Screenshot& screenshot) = default;

Screenshot::~Screenshot() = default;

InstallableData::InstallableData(std::vector<InstallableStatusCode> errors,
                                 const GURL& manifest_url,
                                 const blink::mojom::Manifest& manifest,
                                 const GURL& primary_icon_url,
                                 const SkBitmap* primary_icon,
                                 bool has_maskable_primary_icon,
                                 const std::vector<Screenshot>& screenshots,
                                 bool valid_manifest,
                                 bool worker_check_passed)
    : errors(std::move(errors)),
      manifest_url(manifest_url),
      manifest(manifest),
      primary_icon_url(primary_icon_url),
      primary_icon(primary_icon),
      has_maskable_primary_icon(has_maskable_primary_icon),
      screenshots(screenshots),
      valid_manifest(valid_manifest),
      worker_check_passed(worker_check_passed) {}

InstallableData::~InstallableData() = default;

bool InstallableData::NoBlockingErrors() const {
  return FirstNoBlockingError() == NO_ERROR_DETECTED;
}

InstallableStatusCode InstallableData::FirstNoBlockingError() const {
  for (auto e : errors) {
    switch (e) {
      case WARN_NOT_OFFLINE_CAPABLE:
        continue;
      case NO_MATCHING_SERVICE_WORKER:
#if !BUILDFLAG(IS_ANDROID)
        if (base::FeatureList::IsEnabled(
                features::kCreateShortcutIgnoresManifest)) {
          continue;
        }
#endif
        [[fallthrough]];
      default:
        return e;
    }
  }
  return NO_ERROR_DETECTED;
}

bool InstallableData::HasErrorOnlyServiceWorkerErrors() const {
  if (errors.empty() || errors[0] == NO_ERROR_DETECTED)
    return false;

  for (auto error : errors) {
    if (error != NO_MATCHING_SERVICE_WORKER && error != NOT_OFFLINE_CAPABLE) {
      return false;
    }
  }
  return true;
}

}  // namespace webapps
