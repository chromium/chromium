// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image/image_navigation_throttle.h"

#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ImageNavigationThrottleTest = ::testing::Test;

TEST(ImageNavigationThrottleTest, RegistersForImageUrlsOnly) {
  auto registers_for = [](const std::string& url) {
    content::MockNavigationHandle handle(GURL(url), nullptr);
    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    ImageNavigationThrottle::MaybeCreateAndAdd(registry);
    return registry.ContainsHeldThrottle("ImageNavigationThrottle");
  };

  EXPECT_TRUE(registers_for("chrome://image"));
  EXPECT_TRUE(registers_for("chrome://image?https://example.com"));
  EXPECT_TRUE(registers_for("chrome://image?https://example.com&param=value"));

  EXPECT_FALSE(registers_for("chrome://other"));
  EXPECT_FALSE(registers_for("chrome-untrusted://image"));
  EXPECT_FALSE(registers_for("other://image"));
  EXPECT_FALSE(registers_for("https://www.chromium.org"));
}

TEST(ImageNavigationThrottleTest, BlocksRequests) {
  content::MockNavigationHandle handle(GURL("chrome://image?foo"), nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ImageNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_FALSE(registry.throttles().empty());
  auto& throttle = registry.throttles().front();
  EXPECT_EQ(throttle->WillStartRequest(),
            content::NavigationThrottle::BLOCK_REQUEST);
}
