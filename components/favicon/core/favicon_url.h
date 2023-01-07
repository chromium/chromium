// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_URL_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_URL_H_

#include <vector>

#include "components/favicon_base/favicon_types.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace favicon {

// Stores metadata about a given favicon.
struct FaviconURL {
  FaviconURL();
  FaviconURL(const GURL& url,
             favicon_base::IconType type,
             const std::vector<gfx::Size>& sizes);
  FaviconURL(const FaviconURL& other);
  ~FaviconURL();

  // The url of the icon.
  GURL icon_url;

  // The type of the icon.
  favicon_base::IconType icon_type;

  // Icon's bitmaps' size.
  std::vector<gfx::Size> icon_sizes;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_URL_H_
