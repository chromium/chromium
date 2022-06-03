// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_UTIL_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_UTIL_H_

#include "url/gurl.h"

namespace subresource_redirect {

// Gets the new URL for the compressed version of the image resource to enable
// internal redirects.
GURL GetSubresourceURLForURL(const GURL& original_url);

// Returns whether the url points to compression server origin.
bool IsCompressionServerOrigin(const GURL& url);

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_UTIL_H_
