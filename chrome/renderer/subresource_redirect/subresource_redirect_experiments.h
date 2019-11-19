// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_EXPERIMENTS_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_EXPERIMENTS_H_

#include "url/gurl.h"

namespace subresource_redirect {

// Returns true if the given url matches an included media suffix.
bool ShouldIncludeMediaSuffix(const GURL& url);

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_EXPERIMENTS_H_
