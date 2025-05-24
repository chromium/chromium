// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_SAFE_BROWSING_WEB_APP_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_SAFE_BROWSING_WEB_APP_UTILS_H_

#include <optional>

#include "components/safe_browsing/core/common/proto/csd.pb.h"

class GURL;

namespace safe_browsing {

// Returns a SafeBrowsingWebAppKey proto for a web app with the given
// `start_url` and `manifest_id`. `start_url` should be valid and a non-opaque
// origin. The `manifest_id` should conform to the definition in
// https://www.w3.org/TR/appmanifest/#id-member. If `manifest_id` is not valid,
// the `start_url` will be used instead. If they are both valid, they should be
// same-origin. Returns std::nullopt if any required conditions are not met.
std::optional<SafeBrowsingWebAppKey> GetSafeBrowsingWebAppKey(
    const GURL& start_url,
    const GURL& manifest_id);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_SAFE_BROWSING_WEB_APP_UTILS_H_
