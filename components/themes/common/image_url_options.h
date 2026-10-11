// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_COMMON_IMAGE_URL_OPTIONS_H_
#define COMPONENTS_THEMES_COMMON_IMAGE_URL_OPTIONS_H_

#include <string>

#include "url/gurl.h"

// Retrieve the options to be added to a thumbnail image URL.
std::string GetThumbnailImageOptions();

// Retrieve the options to be added to an image URL.
std::string GetImageOptions();

// Adds options for resizing an image to its url.
// Without options added to the image, it is 512x512.
// Returns an empty GURL if `image_url` is not a valid HTTP(S) URL.
// TODO(crbug.com/41408116): Request resolution from service, instead of
// setting it here.
GURL AddOptionsToImageURL(const std::string& image_url,
                          const std::string& image_options);

// Removes the options for resizing an image from a url. The URL includes the
// options after a single `=` but all still part of the path, so they can't be
// removed with built-in URL modification tools.
GURL RemoveOptionsFromImageURL(const std::string& image_url);

#endif  // COMPONENTS_THEMES_COMMON_IMAGE_URL_OPTIONS_H_
