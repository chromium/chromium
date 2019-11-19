// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/io_buffer_pool.h"

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {
const size_t kDefaultBufferSize = 256;
}  // namespace

TEST(IOBufferPoolTest, ZeroMaxBuffers) {
  auto pool = base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize, 0);
  EXPECT_EQ(nullptr, pool->GetBuffer());
  EXPECT_EQ(kDefaultBufferSize, pool->buffer_size());
  EXPECT_EQ(0u, pool->max_buffers());
  EXPECT_EQ(0u, pool->NumAllocatedForTesting());
  EXPECT_EQ(0u, pool->NumFreeForTesting());
}

TEST(IOBufferPoolTest, OneMaxBuffer) {
  auto pool = base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize, 1);
  scoped_refptr<net::IOBuffer> buffer = pool->GetBuffer();
  EXPECT_NE(nullptr, buffer.get());
  EXPECT_EQ(nullptr, pool->GetBuffer());
  EXPECT_EQ(1u, pool->max_buffers());
  EXPECT_EQ(1u, pool->NumAllocatedForTesting());
  EXPECT_EQ(0u, pool->NumFreeForTesting());
}

TEST(IOBufferPoolTest, SeveralMaxBuffers) {
  auto pool = base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize, 4);
  scoped_refptr<net::IOBuffer> buffer1 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer2 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer3 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer4 = pool->GetBuffer();
  EXPECT_NE(nullptr, buffer1.get());
  EXPECT_NE(nullptr, buffer2.get());
  EXPECT_NE(nullptr, buffer3.get());
  EXPECT_NE(nullptr, buffer4.get());
  EXPECT_EQ(nullptr, pool->GetBuffer());
  EXPECT_EQ(4u, pool->max_buffers());
  EXPECT_EQ(4u, pool->NumAllocatedForTesting());
  EXPECT_EQ(0u, pool->NumFreeForTesting());
}

TEST(IOBufferPoolTest, Reclaim) {
  auto pool = base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize, 3);
  scoped_refptr<net::IOBuffer> buffer1 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer2 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer3 = pool->GetBuffer();
  EXPECT_NE(nullptr, buffer1.get());
  EXPECT_NE(nullptr, buffer2.get());
  EXPECT_NE(nullptr, buffer3.get());
  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(0u, pool->NumFreeForTesting());
  buffer1 = nullptr;
  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(1u, pool->NumFreeForTesting());
  buffer2 = nullptr;
  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(2u, pool->NumFreeForTesting());
  buffer3 = nullptr;
  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(3u, pool->NumFreeForTesting());
  buffer1 = pool->GetBuffer();
  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(2u, pool->NumFreeForTesting());
}

TEST(IOBufferPoolTest, DestroyBufferAfterPool) {
  auto pool = base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize, 3);
  scoped_refptr<net::IOBuffer> buffer1 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer2 = pool->GetBuffer();
  EXPECT_NE(nullptr, buffer1.get());
  EXPECT_NE(nullptr, buffer2.get());
  EXPECT_EQ(2u, pool->NumAllocatedForTesting());
  EXPECT_EQ(0u, pool->NumFreeForTesting());
  buffer1 = nullptr;
  EXPECT_EQ(2u, pool->NumAllocatedForTesting());
  EXPECT_EQ(1u, pool->NumFreeForTesting());

  pool = nullptr;
  buffer2 = nullptr;  // Expect no crash and no memory errors.
}

TEST(IOBufferPoolTest, Preallocate) {
  auto pool = base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize, 3);
  pool->Preallocate(8);

  EXPECT_EQ(3u, pool->max_buffers());
  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(3u, pool->NumFreeForTesting());

  scoped_refptr<net::IOBuffer> buffer1 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer2 = pool->GetBuffer();
  scoped_refptr<net::IOBuffer> buffer3 = pool->GetBuffer();

  EXPECT_NE(nullptr, buffer1.get());
  EXPECT_NE(nullptr, buffer2.get());
  EXPECT_NE(nullptr, buffer3.get());
  EXPECT_EQ(nullptr, pool->GetBuffer());

  EXPECT_EQ(3u, pool->NumAllocatedForTesting());
  EXPECT_EQ(0u, pool->NumFreeForTesting());
}

}  // namespace chromecast
