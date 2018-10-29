// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shared_memory.h"

#include <stddef.h>

#include "components/exo/buffer.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

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
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;

  std::unique_ptr<SharedMemory> shared_memory =
      CreateSharedMemory(gfx::BufferSizeForBufferFormat(buffer_size, format));
  ASSERT_TRUE(shared_memory);

  // Creating a full size buffer should succeed.
  std::unique_ptr<Buffer> buffer = shared_memory->CreateBuffer(
      buffer_size, format, 0,
      gfx::RowSizeForBufferFormat(buffer_size.width(), format, 0));
  EXPECT_TRUE(buffer);

  // Creating a buffer for the top-left rectangle should succeed.
  const gfx::Size top_left_buffer_size(128, 128);
  std::unique_ptr<Buffer> top_left_buffer = shared_memory->CreateBuffer(
      top_left_buffer_size, format, 0,
      gfx::RowSizeForBufferFormat(buffer_size.width(), format, 0));
  EXPECT_TRUE(top_left_buffer);

  // Creating a buffer for the bottom-right rectangle should succeed.
  const gfx::Size bottom_right_buffer_size(64, 64);
  std::unique_ptr<Buffer> bottom_right_buffer = shared_memory->CreateBuffer(
      bottom_right_buffer_size, format,
      (buffer_size.height() - bottom_right_buffer_size.height()) *
              gfx::RowSizeForBufferFormat(buffer_size.width(), format, 0) +
          (buffer_size.width() - bottom_right_buffer_size.width()) * 4,
      gfx::RowSizeForBufferFormat(buffer_size.width(), format, 0));
  EXPECT_TRUE(bottom_right_buffer);
}

}  // namespace
}  // namespace exo
