// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/client_side_detection/client_side_detection_feature_cache.h"

#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class ClientSideDetectionFeatureCacheTest : public PlatformTest {
 protected:
  web::FakeWebState web_state_;
};

// Tests that the cache is properly attached to and retrieved from a WebState.
TEST_F(ClientSideDetectionFeatureCacheTest, CreateAndRetrieve) {
  // Initially, there should be no cache attached.
  EXPECT_EQ(ClientSideDetectionFeatureCache::FromWebState(&web_state_),
            nullptr);

  // Create the cache for the WebState.
  ClientSideDetectionFeatureCache::CreateForWebState(&web_state_);

  // Retrieve the cache.
  ClientSideDetectionFeatureCache* cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  EXPECT_NE(cache, nullptr);

  // Verify that calling Create again doesn't change the instance.
  ClientSideDetectionFeatureCache::CreateForWebState(&web_state_);
  EXPECT_EQ(ClientSideDetectionFeatureCache::FromWebState(&web_state_), cache);
}

}  // namespace safe_browsing
