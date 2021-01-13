// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_FEATURES_H_

namespace subresource_redirect {

// Returns if the public image hints based subresource compression is enabled.
bool ShouldEnablePublicImageHintsBasedCompression();

// Returns if the login and robots checks based subresource compression is
// enabled. This compresses non logged-in pages and subresources allowed by
// robots.txt rules.
bool ShouldEnableLoginRobotsCheckedCompression();

// Should the subresource be redirected to its compressed version. This returns
// false if only coverage metrics need to be recorded and actual redirection
// should not happen.
bool ShouldCompressRedirectSubresource();

}  // namespace subresource_redirect

#endif  // COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_FEATURES_H_
