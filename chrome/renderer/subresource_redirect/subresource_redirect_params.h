// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_

#include <string>

#include "base/time/time.h"
#include "url/origin.h"

namespace subresource_redirect {

// Returns the origin to use for subresource redirect from fieldtrial or the
// default.
url::Origin GetSubresourceRedirectOrigin();

// Returns if the public image hints based subresource compression is enabled.
bool IsPublicImageHintsBasedCompressionEnabled();

// Should the subresource be redirected to its compressed version. This returns
// false if only coverage metrics need to be recorded and actual redirection
// should not happen.
bool ShouldCompressionServerRedirectSubresource();

// Returns the timeout for the compressed subresource redirect, after which the
// subresource should be fetched directly from the origin.
base::TimeDelta GetCompressionRedirectTimeout();

// Returns the public image hinte receive timeout value from field trial.
int64_t GetHintsReceiveTimeout();

// Returns the timeout to wait for the robots rules to be received, after which
// the subresource should be fetched directly from the origin.
base::TimeDelta GetRobotsRulesReceiveTimeout();

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
