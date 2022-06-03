// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_win.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/gmock_callback_support.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceClosure;
using testing::_;

namespace viz {
namespace {

constexpr gfx::Size kDefaultSize(100, 100);

class MockLayeredWindowUpdater : public mojom::LayeredWindowUpdater {
 public:
  MockLayeredWindowUpdater() = default;
  ~MockLayeredWindowUpdater() override = default;

  mojo::PendingRemote<mojom::LayeredWindowUpdater> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  // mojom::LayeredWindowUpdater implementation.
  MOCK_METHOD(void,
              OnAllocatedSharedMemory,
              (const gfx::Size& size,
               base::UnsafeSharedMemoryRegion memory_region),
              (override));
  MOCK_METHOD(void, Draw, (DrawCallback), (override));

 private:
  mojo::Receiver<mojom::LayeredWindowUpdater> receiver_{this};
};

}  // namespace

class SoftwareOutputDeviceWinProxyTest : public testing::Test {
 public:
  SoftwareOutputDeviceWinProxyTest()
      : device_(0, updater_.BindNewPipeAndPassRemote()) {}

  void SetUp() override {
    // Check that calling Resize() results in the allocation of shared memory
    // and triggers the OnAllocatedSharedMemory() IPC.
    EXPECT_CALL(updater_, OnAllocatedSharedMemory(kDefaultSize, _))
        .WillOnce(
            testing::WithArg<1>([](base::UnsafeSharedMemoryRegion region) {
              EXPECT_TRUE(region.IsValid());
              size_t required_bytes = ResourceSizes::CheckedSizeInBytes<size_t>(
                  kDefaultSize, ResourceFormat::RGBA_8888);
              EXPECT_GE(region.GetSize(), required_bytes);
            }));
    device_.Resize(kDefaultSize, 1.0f);
    updater_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&updater_);
  }

 protected:
  MockLayeredWindowUpdater updater_;
  SoftwareOutputDeviceWinProxy device_;
};

TEST_F(SoftwareOutputDeviceWinProxyTest, DrawWithSwap) {
  // Verify that calling BeginPaint() then EndPaint() triggers the Draw() IPC
  // and then run the DrawCallback.
  device_.BeginPaint(gfx::Rect(kDefaultSize));
  EXPECT_CALL(updater_, Draw(_)).WillOnce(RunOnceClosure<0>());
  device_.EndPaint();
  updater_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&updater_);

  // OnSwapBuffers() is called before DrawAck() so the swap buffers callback
  // shouldn't run yet.
  bool called = false;
  device_.OnSwapBuffers(base::BindOnce(
      [](bool* val, const gfx::Size& size) { *val = true; }, &called));
  EXPECT_FALSE(called);

  // Verify that DrawAck() runs the swap buffers callback.
  updater_.FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(SoftwareOutputDeviceWinProxyTest, DrawNoSwap) {
  // Verify that calling BeginPaint() then EndPaint() triggers the Draw() IPC
  // and then run the DrawCallback.
  device_.BeginPaint(gfx::Rect(kDefaultSize));
  EXPECT_CALL(updater_, Draw(_)).WillOnce(RunOnceClosure<0>());
  device_.EndPaint();
  updater_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&updater_);

  // DrawAck() will be triggered after this second flush when the IPC response
  // arrives. OnSwapBuffers() was never called which is allowed.
  updater_.FlushForTesting();
}

}  // namespace viz
