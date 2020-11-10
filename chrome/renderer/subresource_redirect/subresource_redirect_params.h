// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_

#include <string>

#include "url/origin.h"

namespace subresource_redirect {

// Returns the origin to use for subresource redirect from fieldtrial or the
// default.
url::Origin GetSubresourceRedirectOrigin();

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
