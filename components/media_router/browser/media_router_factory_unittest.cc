// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_factory.h"

#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media_router {

class TestMediaRouterFactory : public MediaRouterFactory {
 public:
  TestMediaRouterFactory() = default;
  ~TestMediaRouterFactory() override = default;

  MOCK_METHOD(KeyedService*,
              BuildServiceInstanceFor,
              (content::BrowserContext* context),
              (const));
};

class MediaRouterFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestMediaRouterFactory factory_;
  content::TestBrowserContext context_;
};

TEST_F(MediaRouterFactoryTest, NullMediaRouterForNullBrowserContext) {
  EXPECT_EQ(nullptr, factory_.GetApiForBrowserContextIfExists(nullptr));
}

TEST_F(MediaRouterFactoryTest, NullMediaRouterWhenInstanceDoesntExist) {
  // If a MediaRouter instance doesn't exist, GetApiForBrowserContextIfExists()
  // shouldn't result in an instance getting created or returned.
  EXPECT_CALL(factory_, BuildServiceInstanceFor(_)).Times(0);
  EXPECT_EQ(nullptr, factory_.GetApiForBrowserContextIfExists(&context_));
}

}  // namespace media_router
