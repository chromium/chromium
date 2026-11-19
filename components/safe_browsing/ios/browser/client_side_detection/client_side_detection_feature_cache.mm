// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/ios/browser/client_side_detection/client_side_detection_feature_cache.h"

#include "ios/web/public/web_state.h"

using web::WebState;

namespace safe_browsing {

ClientSideDetectionFeatureCache::ClientSideDetectionFeatureCache(
    web::WebState* web_state) {}

ClientSideDetectionFeatureCache::~ClientSideDetectionFeatureCache() = default;

}  // namespace safe_browsing
