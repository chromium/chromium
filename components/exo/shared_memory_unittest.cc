// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shared_memory.h"

#include <stddef.h>

#include "components/exo/buffer.h"
#include "components/exo/test/exo_test_base.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using SharedMemoryTest = test::ExoTestBase;

std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) {
  base::UnsafeSharedMemoryRegion shared_memory =
      base::UnsafeSharedMemoryRegion::Create(size);
  DCHECK(shared_memory.IsValid());
  return std::make_unique<SharedMemory>(std::move(shared_memory));
}

TEST_F(SharedMemoryTest, CreateBuffer) {
  const gfx::Size buffer_size(256, 256);
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;

  std::unique_ptr<SharedMemory> shared_memory = CreateSharedMemory(
      viz::SharedMemorySizeForSharedImageFormat(format, buffer_size).value());
  ASSERT_TRUE(shared_memory);

  // Creating a full size buffer should succeed.
  uint32_t bytes_per_row = viz::SharedMemoryRowSizeForSharedImageFormat(
                               format, 0, buffer_size.width())
                               .value();
  std::unique_ptr<Buffer> buffer =
      shared_memory->CreateBuffer(buffer_size, format, 0, bytes_per_row);
  EXPECT_TRUE(buffer);

  // Creating a buffer for the top-left rectangle should succeed.
  const gfx::Size top_left_buffer_size(128, 128);
  uint32_t top_left_buffer_size_width =
      viz::SharedMemoryRowSizeForSharedImageFormat(format, 0,
                                                   top_left_buffer_size.width())
          .value();
  std::unique_ptr<Buffer> top_left_buffer = shared_memory->CreateBuffer(
      top_left_buffer_size, format, 0, top_left_buffer_size_width);
  EXPECT_TRUE(top_left_buffer);

  // Creating a buffer for the bottom-right rectangle should succeed.
  const gfx::Size bottom_right_buffer_size(64, 64);
  uint32_t bottom_right_buffer_size_width =
      viz::SharedMemoryRowSizeForSharedImageFormat(
          format, 0, bottom_right_buffer_size.width())
          .value();
  std::unique_ptr<Buffer> bottom_right_buffer = shared_memory->CreateBuffer(
      bottom_right_buffer_size, format,
      (buffer_size.height() - bottom_right_buffer_size.height()) *
              bytes_per_row +
          (buffer_size.width() - bottom_right_buffer_size.width()) * 4,
      bottom_right_buffer_size_width);
  EXPECT_TRUE(bottom_right_buffer);
}

}  // namespace
}  // namespace exo
