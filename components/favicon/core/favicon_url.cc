// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_url.h"

namespace favicon {

FaviconURL::FaviconURL() : icon_type(favicon_base::IconType::kInvalid) {}

FaviconURL::FaviconURL(const GURL& url,
                       favicon_base::IconType type,
                       const std::vector<gfx::Size>& sizes)
    : icon_url(url), icon_type(type), icon_sizes(sizes) {
}

FaviconURL::FaviconURL(const FaviconURL& other) = default;

FaviconURL::~FaviconURL() {
}

}  // namespace favicon
