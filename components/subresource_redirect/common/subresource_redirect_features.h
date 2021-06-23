// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_FEATURES_H_

#include "url/origin.h"

namespace subresource_redirect {

// Returns if the public image hints based subresource compression is enabled.
bool ShouldEnablePublicImageHintsBasedCompression();

// Returns if the login and robots checks based image compression is enabled.
// This compresses images in non logged-in pages allowed by robots.txt rules.
bool ShouldEnableLoginRobotsCheckedImageCompression();

// Returns if the login and robots checks based src-video metrics recording is
// enabled. This only records data use and coverage metrics for src videos on
// non logged-in pages allowed by robots.txt rules.
bool ShouldRecordLoginRobotsCheckedSrcVideoMetrics();

// Should the subresource be redirected to its compressed version. This returns
// false if only coverage metrics need to be recorded and actual redirection
// should not happen.
bool ShouldCompressRedirectSubresource();

// Returns whether robots rules can be fetched. Robots rules fetching is enabled
// when certain features are active, such as robots and login checked image
// and src-video compression.
bool ShouldEnableRobotsRulesFetching();

// Returns the origin to use for subresource redirect from fieldtrial or the
// default.
url::Origin GetSubresourceRedirectOrigin();

}  // namespace subresource_redirect

#endif  // COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_FEATURES_H_
