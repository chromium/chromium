// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBUI_WEBUI_BUNDLED_CODE_CACHE_MAP_H_
#define CHROME_RENDERER_WEBUI_WEBUI_BUNDLED_CODE_CACHE_MAP_H_

#include <optional>

class GURL;

namespace webui {

// Returns the bundled code cache resource ID for `webui_resource_url`, or
// `std::nullopt` if none exists or bundled code cache generation is disabled at
// build time. Thread-safe.
std::optional<int> GetWebUIBundledCodeCacheResourceId(
    const GURL& webui_resource_url);

}  // namespace webui

#endif  // CHROME_RENDERER_WEBUI_WEBUI_BUNDLED_CODE_CACHE_MAP_H_
