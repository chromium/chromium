// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection/client_side_detection_feature_cache.h"

#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ClientSideDetectionFeatureCacheTest = content::RenderViewHostTestHarness;

TEST_F(ClientSideDetectionFeatureCacheTest, CreateAndRetrieve) {
  content::WebContents* content = web_contents();

  // Initially, there should be no cache attached.
  EXPECT_EQ(ClientSideDetectionFeatureCache::FromWebContents(content), nullptr);

  // Create the cache for the WebContents.
  ClientSideDetectionFeatureCache::CreateForWebContents(content);

  // Retrieve the cache.
  ClientSideDetectionFeatureCache* cache =
      ClientSideDetectionFeatureCache::FromWebContents(content);
  EXPECT_NE(cache, nullptr);

  // Verify that calling Create again doesn't change the instance.
  ClientSideDetectionFeatureCache::CreateForWebContents(content);
  EXPECT_EQ(ClientSideDetectionFeatureCache::FromWebContents(content), cache);
}

}  // namespace safe_browsing
