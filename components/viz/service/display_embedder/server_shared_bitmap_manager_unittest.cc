// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"

#include "base/containers/span.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class ServerSharedBitmapManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    manager_ = std::make_unique<ServerSharedBitmapManager>();
  }

  void TearDown() override { manager_.reset(); }

  ServerSharedBitmapManager* manager() const { return manager_.get(); }

 private:
  std::unique_ptr<ServerSharedBitmapManager> manager_;
};

TEST_F(ServerSharedBitmapManagerTest, TestCreate) {
  gfx::Size bitmap_size(1, 1);
  base::MappedReadOnlyRegion shm =
      bitmap_allocation::AllocateSharedBitmap(bitmap_size, RGBA_8888);
  EXPECT_TRUE(shm.IsValid());
  base::span<uint8_t> span = shm.mapping.GetMemoryAsSpan<uint8_t>();
  std::fill(span.begin(), span.end(), 0xff);

  SharedBitmapId id = SharedBitmap::GenerateId();
  manager()->ChildAllocatedSharedBitmap(shm.region.Map(), id);

  std::unique_ptr<SharedBitmap> large_bitmap;
  large_bitmap =
      manager()->GetSharedBitmapFromId(gfx::Size(1024, 1024), RGBA_8888, id);
  EXPECT_FALSE(large_bitmap);

  std::unique_ptr<SharedBitmap> very_large_bitmap;
  very_large_bitmap = manager()->GetSharedBitmapFromId(
      gfx::Size(1, (1 << 30) | 1), RGBA_8888, id);
  EXPECT_FALSE(very_large_bitmap);

  std::unique_ptr<SharedBitmap> negative_size_bitmap;
  negative_size_bitmap =
      manager()->GetSharedBitmapFromId(gfx::Size(-1, 1024), RGBA_8888, id);
  EXPECT_FALSE(negative_size_bitmap);

  SharedBitmapId id2 = SharedBitmap::GenerateId();
  std::unique_ptr<SharedBitmap> invalid_bitmap;
  invalid_bitmap =
      manager()->GetSharedBitmapFromId(bitmap_size, RGBA_8888, id2);
  EXPECT_FALSE(invalid_bitmap);

  std::unique_ptr<SharedBitmap> shared_bitmap;
  shared_bitmap = manager()->GetSharedBitmapFromId(bitmap_size, RGBA_8888, id);
  ASSERT_TRUE(shared_bitmap);
  EXPECT_TRUE(
      std::equal(span.begin(), span.begin() + 4, shared_bitmap->pixels()));

  std::unique_ptr<SharedBitmap> large_bitmap2;
  large_bitmap2 =
      manager()->GetSharedBitmapFromId(gfx::Size(1024, 1024), RGBA_8888, id);
  EXPECT_FALSE(large_bitmap2);

  std::unique_ptr<SharedBitmap> shared_bitmap2;
  shared_bitmap2 = manager()->GetSharedBitmapFromId(bitmap_size, RGBA_8888, id);
  EXPECT_TRUE(shared_bitmap2->pixels() == shared_bitmap->pixels());
  shared_bitmap2.reset();
  EXPECT_TRUE(std::equal(span.begin(), span.end(), shared_bitmap->pixels()));

  manager()->ChildDeletedSharedBitmap(id);

  std::fill(span.begin(), span.end(), 0);

  EXPECT_TRUE(std::equal(span.begin(), span.end(), shared_bitmap->pixels()));
  shared_bitmap.reset();
}

TEST_F(ServerSharedBitmapManagerTest, AddDuplicate) {
  gfx::Size bitmap_size(1, 1);
  base::MappedReadOnlyRegion shm =
      bitmap_allocation::AllocateSharedBitmap(bitmap_size, RGBA_8888);
  EXPECT_TRUE(shm.IsValid());
  base::span<uint8_t> span = shm.mapping.GetMemoryAsSpan<uint8_t>();
  std::fill(span.begin(), span.end(), 0xff);
  SharedBitmapId id = SharedBitmap::GenerateId();

  // NOTE: Duplicate the mapping to compare its content later.
  manager()->ChildAllocatedSharedBitmap(shm.region.Map(), id);

  base::MappedReadOnlyRegion shm2 =
      bitmap_allocation::AllocateSharedBitmap(bitmap_size, RGBA_8888);
  EXPECT_TRUE(shm2.IsValid());
  base::span<uint8_t> span2 = shm.mapping.GetMemoryAsSpan<uint8_t>();
  std::fill(span2.begin(), span2.end(), 0x00);

  manager()->ChildAllocatedSharedBitmap(shm2.region.Map(), id);

  std::unique_ptr<SharedBitmap> shared_bitmap;
  shared_bitmap = manager()->GetSharedBitmapFromId(bitmap_size, RGBA_8888, id);
  ASSERT_TRUE(shared_bitmap);
  EXPECT_TRUE(std::equal(span.begin(), span.end(), shared_bitmap->pixels()));
  manager()->ChildDeletedSharedBitmap(id);
}

TEST_F(ServerSharedBitmapManagerTest, SharedMemoryHandle) {
  gfx::Size bitmap_size(1, 1);
  base::MappedReadOnlyRegion shm =
      bitmap_allocation::AllocateSharedBitmap(bitmap_size, RGBA_8888);
  EXPECT_TRUE(shm.IsValid());
  base::span<uint8_t> span = shm.mapping.GetMemoryAsSpan<uint8_t>();
  std::fill(span.begin(), span.end(), 0xff);
  base::UnguessableToken shared_memory_guid = shm.mapping.guid();
  EXPECT_FALSE(shared_memory_guid.is_empty());

  SharedBitmapId id = SharedBitmap::GenerateId();
  manager()->ChildAllocatedSharedBitmap(shm.region.Map(), id);

  base::UnguessableToken tracing_guid =
      manager()->GetSharedBitmapTracingGUIDFromId(id);
  EXPECT_EQ(tracing_guid, shared_memory_guid);

  manager()->ChildDeletedSharedBitmap(id);
}

TEST_F(ServerSharedBitmapManagerTest, InvalidScopedSharedBufferHandle) {
  SharedBitmapId id = SharedBitmap::GenerateId();
  base::ReadOnlySharedMemoryMapping invalid_mapping;
  EXPECT_FALSE(invalid_mapping.IsValid());
  EXPECT_FALSE(
      manager()->ChildAllocatedSharedBitmap(std::move(invalid_mapping), id));

  // The client could still send an IPC to say it deleted the shared bitmap,
  // even though it wasn't valid, which should be ignored.
  manager()->ChildDeletedSharedBitmap(id);
}

}  // namespace
}  // namespace viz
