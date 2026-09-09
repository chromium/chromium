// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_UTIL_H_
#define COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_UTIL_H_

#include "ui/gfx/image/image.h"

namespace content {
class WebContents;
}  // namespace content

namespace favicon {

// Returns the favicon of the given WebContents' last committed page. If that
// page is a network error, the returned favicon is desaturated.
gfx::Image GetTabFaviconMaybeDesaturatedOnError(content::WebContents* contents);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_UTIL_H_
