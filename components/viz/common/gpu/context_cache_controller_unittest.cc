// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/context_cache_controller.h"

#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/gpu/GrContext.h"

using ::testing::Mock;
using ::testing::StrictMock;

namespace viz {
namespace {

class MockContextSupport : public TestContextSupport {
 public:
  MockContextSupport() {}
  MOCK_METHOD1(SetAggressivelyFreeResources,
               void(bool aggressively_free_resources));
};

TEST(ContextCacheControllerTest, ScopedVisibilityBasic) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  ContextCacheController cache_controller(&context_support, task_runner);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  std::unique_ptr<ContextCacheController::ScopedVisibility> visibility =
      cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visibility));
}

TEST(ContextCacheControllerTest, ScopedVisibilityMulti) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  ContextCacheController cache_controller(&context_support, task_runner);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  auto visibility_1 = cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);
  auto visibility_2 = cache_controller.ClientBecameVisible();

  cache_controller.ClientBecameNotVisible(std::move(visibility_1));
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visibility_2));
}

// Check that resources aren't freed during shutdown until the
// ContextCacheController is also deleted.
TEST(ContextCacheControllerTest, ScopedVisibilityDuringShutdown) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  std::unique_ptr<ContextCacheController> cache_controller =
      std::make_unique<ContextCacheController>(&context_support, task_runner);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  std::unique_ptr<ContextCacheController::ScopedVisibility> visibility =
      cache_controller->ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);

  cache_controller->ClientBecameNotVisibleDuringShutdown(std::move(visibility));

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.reset();
}

// Check that multiple clients can shutdown successfully.
TEST(ContextCacheControllerTest, ScopedVisibilityDuringShutdownMulti) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  std::unique_ptr<ContextCacheController> cache_controller =
      std::make_unique<ContextCacheController>(&context_support, task_runner);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  auto visibility_1 = cache_controller->ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);
  auto visibility_2 = cache_controller->ClientBecameVisible();

  cache_controller->ClientBecameNotVisibleDuringShutdown(
      std::move(visibility_1));
  cache_controller->ClientBecameNotVisibleDuringShutdown(
      std::move(visibility_2));

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.reset();
}

TEST(ContextCacheControllerTest, ScopedBusyWhileVisible) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  ContextCacheController cache_controller(&context_support, task_runner);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  auto visibility = cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);

  // Now that we're visible, ensure that going idle triggers a delayed cleanup.
  auto busy = cache_controller.ClientBecameBusy();
  cache_controller.ClientBecameNotBusy(std::move(busy));

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
  Mock::VerifyAndClearExpectations(&context_support);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visibility));
}

TEST(ContextCacheControllerTest, ScopedBusyWhileNotVisible) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  ContextCacheController cache_controller(&context_support, task_runner);

  auto busy = cache_controller.ClientBecameBusy();

  // We are not visible, so becoming busy should not trigger an idle callback.
  cache_controller.ClientBecameNotBusy(std::move(busy));
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
}

TEST(ContextCacheControllerTest, ScopedBusyMulitpleWhileVisible) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  ContextCacheController cache_controller(&context_support, task_runner);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  auto visible = cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);

  auto busy_1 = cache_controller.ClientBecameBusy();
  cache_controller.ClientBecameNotBusy(std::move(busy_1));
  auto busy_2 = cache_controller.ClientBecameBusy();
  cache_controller.ClientBecameNotBusy(std::move(busy_2));

  // When we fast forward, only one cleanup should happen.
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
  Mock::VerifyAndClearExpectations(&context_support);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visible));
}

// Confirms that the Skia performDeferredCleanup API used by the cache
// controller behaves as expected.
TEST(ContextCacheControllerTest, CheckSkiaResourcePurgeAPI) {
  StrictMock<MockContextSupport> context_support;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  ContextCacheController cache_controller(&context_support, task_runner);
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  auto* gr_context = context_provider->GrContext();
  cache_controller.SetGrContext(gr_context);

  // Make us visible.
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  auto visibility = cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);

  // Now that we're visible, become busy, create and release a skia resource.
  auto busy = cache_controller.ClientBecameBusy();
  {
    auto image_info = SkImageInfo::MakeN32Premul(200, 200);
    std::vector<uint8_t> image_data(image_info.computeMinByteSize());
    SkPixmap pixmap(image_info, image_data.data(), image_info.minRowBytes());
    auto image = SkImage::MakeRasterCopy(pixmap);
    auto image_gpu = image->makeTextureImage(gr_context);
    gr_context->flush();
  }

  // Ensure we see size taken up for the image (now released, but cached for
  // re-use).
  EXPECT_GT(gr_context->getResourceCachePurgeableBytes(), 0u);

  // Make the client idle and wait for the idle callback to trigger.
  cache_controller.ClientBecameNotBusy(std::move(busy));
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
  Mock::VerifyAndClearExpectations(&context_support);

  // The Skia resource cache should now be empty.
  EXPECT_EQ(gr_context->getResourceCachePurgeableBytes(), 0u);

  // Set not-visible.
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visibility));
}

}  // namespace
}  // namespace viz
