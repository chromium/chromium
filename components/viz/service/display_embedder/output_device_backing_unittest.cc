// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_device_backing.h"

#include <limits>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

size_t GetViewportSizeInBytes(const gfx::Size& viewport_size) {
  size_t bytes = std::numeric_limits<size_t>::max();
  CHECK(ResourceSizes::MaybeSizeInBytes(viewport_size,
                                        SinglePlaneFormat::kRGBA_8888, &bytes));
  return bytes;
}

// Test implementation with a set viewport size.
class TestBackingClient : public OutputDeviceBacking::Client {
 public:
  TestBackingClient(OutputDeviceBacking* backing,
                    const gfx::Size& viewport_size)
      : backing_(backing), viewport_size_(viewport_size) {
    backing_->RegisterClient(this);
    backing_->ClientResized();
    backing_->GetSharedMemoryRegion(viewport_size_);
  }

  TestBackingClient(const TestBackingClient&) = delete;
  TestBackingClient& operator=(const TestBackingClient&) = delete;

  ~TestBackingClient() override { backing_->UnregisterClient(this); }

  const gfx::Size& viewport_size() const { return viewport_size_; }
  bool release_canvas_called() const { return release_canvas_called_; }

  // OutputDeviceBacking::Client implementation.
  const gfx::Size& GetViewportPixelSize() const override {
    return viewport_size_;
  }
  void ReleaseCanvas() override { release_canvas_called_ = true; }

 private:
  const raw_ptr<OutputDeviceBacking> backing_;
  gfx::Size viewport_size_;
  bool release_canvas_called_ = false;
};

}  // namespace

// Verify GetMaxViewportBytes() returns the size in bytes of the largest
// viewport.
TEST(OutputDeviceBackingTest, GetMaxViewportBytes) {
  OutputDeviceBacking backing;
  TestBackingClient client_a(&backing, gfx::Size(1024, 768));
  TestBackingClient client_b(&backing, gfx::Size(1920, 1080));

  EXPECT_EQ(GetViewportSizeInBytes(client_b.viewport_size()),
            backing.GetMaxViewportBytes());
}

// Verify that unregistering a client works as expected.
TEST(OutputDeviceBackingTest, UnregisterClient) {
  OutputDeviceBacking backing;
  auto client_a =
      std::make_unique<TestBackingClient>(&backing, gfx::Size(1920, 1080));
  auto client_b =
      std::make_unique<TestBackingClient>(&backing, gfx::Size(1080, 1920));
  auto client_c =
      std::make_unique<TestBackingClient>(&backing, gfx::Size(1024, 768));

  // Unregister one of the clients with the 1920x1080 viewport size.
  client_a.reset();

  // After removing |client_a| the max viewport didn't change so ReleaseCanvas()
  // shouldn't be have been called.
  EXPECT_FALSE(client_b->release_canvas_called());
  EXPECT_FALSE(client_c->release_canvas_called());

  // Unregister the second client with 1920x1080 viewport.
  client_b.reset();

  // After removing |client_b| then max viewport did change so ReleaseCanvas()
  // should have been called.
  EXPECT_TRUE(client_c->release_canvas_called());

  EXPECT_EQ(GetViewportSizeInBytes(client_c->viewport_size()),
            backing.GetMaxViewportBytes());
}

// Verify that a client with a viewport that is too large doesn't allocate
// SharedMemory.
TEST(OutputDeviceBackingTest, ViewportSizeBiggerThanMax) {
  OutputDeviceBacking backing;

  // The viewport is bigger than |kMaxBitmapSizeBytes| and OutputDeviceBacking
  // won't try to create a shared memory region for it.
  TestBackingClient client_a(&backing, gfx::Size(16385, 8193));
  EXPECT_EQ(nullptr, backing.GetSharedMemoryRegion(client_a.viewport_size()));

  // This should cause a region to get allocated.
  TestBackingClient client_b(&backing, gfx::Size(1024, 768));
  EXPECT_NE(nullptr, backing.GetSharedMemoryRegion(client_b.viewport_size()));

  // Even though SharedMemory was allocated to fit |client_b|, it will be too
  // small for |client_a| and GetSharedMemoryRegion() should still return null.
  EXPECT_EQ(nullptr, backing.GetSharedMemoryRegion(client_a.viewport_size()));
}

}  // namespace viz
