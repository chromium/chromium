// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection/client_side_detection_feature_cache.h"

#include "content/public/browser/web_contents_user_data.h"

using content::WebContents;

namespace safe_browsing {

ClientSideDetectionFeatureCache::ClientSideDetectionFeatureCache(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ClientSideDetectionFeatureCache>(
          *web_contents) {}

ClientSideDetectionFeatureCache::~ClientSideDetectionFeatureCache() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientSideDetectionFeatureCache);

}  // namespace safe_browsing
