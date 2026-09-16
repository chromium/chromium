// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/chunk_queue.h"

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <string>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface MultiRangeData : NSData {
 @private
  std::vector<std::string> _ranges;
}

- (instancetype)initWithRanges:(std::vector<std::string>)ranges;
@end

@implementation MultiRangeData

- (instancetype)initWithRanges:(std::vector<std::string>)ranges {
  if ((self = [super init])) {
    _ranges = std::move(ranges);
  }
  return self;
}

- (id)copyWithZone:(NSZone*)zone {
  return self;
}

- (NSUInteger)length {
  NSUInteger total = 0;
  for (const auto& r : _ranges) {
    total += r.length();
  }
  return total;
}

- (const void*)bytes {
  NOTREACHED();
}

- (void)enumerateByteRangesUsingBlock:
    (void(NS_NOESCAPE ^)(const void* bytes, NSRange byteRange, BOOL* stop))
        block {
  NSUInteger current_offset = 0;
  for (const auto& r : _ranges) {
    BOOL stop = NO;
    block(r.data(), NSMakeRange(current_offset, r.length()), &stop);
    if (stop) {
      break;
    }
    current_offset += r.length();
  }
}
@end

namespace updater {

TEST(ChunkQueueTest, PushAndEmpty) {
  ChunkQueue queue;
  EXPECT_TRUE(queue.empty());

  NSData* data = [@"hello" dataUsingEncoding:NSUTF8StringEncoding];
  queue.Push(data);
  EXPECT_FALSE(queue.empty());
}

TEST(ChunkQueueTest, ConsumeAll) {
  ChunkQueue queue;
  queue.Push([@"hello" dataUsingEncoding:NSUTF8StringEncoding]);
  queue.Push([@" world" dataUsingEncoding:NSUTF8StringEncoding]);

  std::string consumed_data;
  MojoResult result = queue.Consume(base::BindLambdaForTesting(
      [&consumed_data](base::span<const uint8_t> slice, size_t& bytes_written) {
        consumed_data.append_range(base::as_chars(slice));
        bytes_written = slice.size();
        return MOJO_RESULT_OK;
      }));

  EXPECT_EQ(result, MOJO_RESULT_OK);
  EXPECT_EQ(consumed_data, "hello world");
  EXPECT_TRUE(queue.empty());
}

TEST(ChunkQueueTest, ConsumePartial) {
  ChunkQueue queue;
  queue.Push([@"hello" dataUsingEncoding:NSUTF8StringEncoding]);

  std::string consumed_data;
  // First call consumes 2 bytes.
  MojoResult result = queue.Consume(base::BindLambdaForTesting(
      [&consumed_data](base::span<const uint8_t> slice, size_t& bytes_written) {
        bytes_written = std::min<size_t>(slice.size(), 2);
        consumed_data.append_range(base::as_chars(slice.first(bytes_written)));
        return MOJO_RESULT_OK;
      }));

  EXPECT_EQ(result, MOJO_RESULT_SHOULD_WAIT);
  EXPECT_EQ(consumed_data, "he");
  EXPECT_FALSE(queue.empty());

  // Second call consumes the rest.
  result = queue.Consume(base::BindLambdaForTesting(
      [&consumed_data](base::span<const uint8_t> slice, size_t& bytes_written) {
        bytes_written = slice.size();
        consumed_data.append_range(base::as_chars(slice));
        return MOJO_RESULT_OK;
      }));

  EXPECT_EQ(result, MOJO_RESULT_OK);
  EXPECT_EQ(consumed_data, "hello");
  EXPECT_TRUE(queue.empty());
}

TEST(ChunkQueueTest, ConsumeBlocked) {
  ChunkQueue queue;
  queue.Push([@"hello" dataUsingEncoding:NSUTF8StringEncoding]);

  // First call returns SHOULD_WAIT.
  MojoResult result = queue.Consume(base::BindRepeating(
      [](base::span<const uint8_t> slice, size_t& bytes_written) {
        bytes_written = 0;
        return MOJO_RESULT_SHOULD_WAIT;
      }));

  EXPECT_EQ(result, MOJO_RESULT_SHOULD_WAIT);
  EXPECT_FALSE(queue.empty());
}

TEST(ChunkQueueTest, ConsumeError) {
  ChunkQueue queue;
  queue.Push([@"hello" dataUsingEncoding:NSUTF8StringEncoding]);

  MojoResult result = queue.Consume(base::BindRepeating(
      [](base::span<const uint8_t> slice, size_t& bytes_written) {
        bytes_written = 0;
        return MOJO_RESULT_INVALID_ARGUMENT;
      }));

  EXPECT_EQ(result, MOJO_RESULT_INVALID_ARGUMENT);
  EXPECT_FALSE(queue.empty());
}

TEST(ChunkQueueTest, ConsumeMultiRangeData) {
  ChunkQueue queue;
  MultiRangeData* data = [[MultiRangeData alloc] initWithRanges:{"abc", "def"}];
  queue.Push(data);

  std::string consumed_data;
  // First call consumes 4 bytes.
  MojoResult result = queue.Consume(base::BindLambdaForTesting(
      [&consumed_data](base::span<const uint8_t> slice, size_t& bytes_written) {
        bytes_written =
            std::min<size_t>(slice.size(), 4 - consumed_data.size());
        consumed_data.append_range(base::as_chars(slice.first(bytes_written)));
        return MOJO_RESULT_OK;
      }));

  EXPECT_EQ(result, MOJO_RESULT_SHOULD_WAIT);
  EXPECT_EQ(consumed_data, "abcd");
  EXPECT_FALSE(queue.empty());

  // Second call consumes the rest.
  result = queue.Consume(base::BindLambdaForTesting(
      [&consumed_data](base::span<const uint8_t> slice, size_t& bytes_written) {
        bytes_written = slice.size();
        consumed_data.append_range(base::as_chars(slice));
        return MOJO_RESULT_OK;
      }));

  EXPECT_EQ(result, MOJO_RESULT_OK);
  EXPECT_EQ(consumed_data, "abcdef");
  EXPECT_TRUE(queue.empty());
}

}  // namespace updater
