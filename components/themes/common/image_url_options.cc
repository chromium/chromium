// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/common/image_url_options.h"

#include "build/build_config.h"

namespace {
#if BUILDFLAG(IS_IOS)
// The iOS options to be added to a thumbnail image URL, specifying resolution,
// cropping, etc. Options appear on an image URL after the '=' character. This
// resolution matches the height an width of bg-sel-tile.
constexpr char kThumbnailImageOptions[] = "=s639-k-no-nd";
// The iOS options to be added to an image URL, specifying resolution, cropping,
// etc. Options appear on an image URL after the '=' character.
constexpr char kImageOptions[] = "=s2556-k-no-nd";
#elif BUILDFLAG(IS_ANDROID)
// The Android options to be added to a thumbnail image URL, specifying
// resolution, cropping, etc. Options appear on an image URL after the '='
// character. This resolution matches the height an width of bg-sel-tile.
constexpr char kThumbnailImageOptions[] = "=w156-h117-p-k-no-nd-mv";
// The Android options to be added to an image URL, specifying resolution,
// cropping, etc. Options appear on an image URL after the '=' character.
constexpr char kImageOptions[] = "=s2556-k-no-nd";
#else
// The desktop options to be added to a thumbnail image URL, specifying
// resolution, cropping, etc. Options appear on an image URL after the '='
// character. This resolution matches the height an width of bg-sel-tile.
constexpr char kThumbnailImageOptions[] = "=w156-h117-p-k-no-nd-mv";
// The desktop options to be added to an image URL, specifying resolution,
// cropping, etc. Options appear on an image URL after the '=' character.
// TODO(crbug.com/41408116): Set options based on display resolution capability.
constexpr char kImageOptions[] = "=w3840-h2160-p-k-no-nd-mv";
#endif

}  // namespace

std::string GetThumbnailImageOptions() {
  return kThumbnailImageOptions;
}

std::string GetImageOptions() {
  return kImageOptions;
}

GURL AddOptionsToImageURL(const std::string& image_url,
                          const std::string& image_options) {
  GURL url(image_url + ((image_url.find('=') == std::string::npos)
                            ? image_options
                            : std::string("")));
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return GURL();
  }
  return url;
}

GURL RemoveOptionsFromImageURL(const std::string& image_url) {
  return GURL((image_url.rfind('=') == std::string::npos)
                  ? image_url
                  : image_url.substr(0, image_url.rfind('=')));
}
