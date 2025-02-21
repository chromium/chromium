// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "components/persistent_cache/mock/mock_entry_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace persistent_cache {

namespace {

constexpr uint8_t kContentRaw[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr base::span kContent(kContentRaw);

}  // namespace

TEST(PersistentCacheEntry, NoFunctionsCalledAfterConstruction) {
  StrictMock<MockEntryImpl> mock_entry;
}

TEST(PersistentCacheEntry, Size) {
  StrictMock<MockEntryImpl> mock_entry;
  EXPECT_CALL(mock_entry, GetContentSpan()).WillOnce(Return(kContent));
  ASSERT_EQ(mock_entry.GetContentSize(), kContent.size());
}

TEST(PersistentCacheEntry, CopyContentToWhenEmpty) {
  StrictMock<MockEntryImpl> mock_entry;
  EXPECT_CALL(mock_entry, GetContentSpan())
      .WillRepeatedly(Return(base::as_byte_span(kContent)));

  std::vector<uint8_t> target;
  // No attempt to copy more than there is room for when target span has no
  // capacity.
  ASSERT_EQ(mock_entry.CopyContentTo(base::span(target)), size_t(0));
}

TEST(PersistentCacheEntry, CopyContentPartial) {
  StrictMock<MockEntryImpl> mock_entry;
  EXPECT_CALL(mock_entry, GetContentSpan())
      .WillRepeatedly(Return(base::as_byte_span(kContent)));

  std::vector<uint8_t> target;

  // No attempt to copy more than there is room for when there is non-zero but
  // insufficient capacity.
  const size_t target_copy_size = kContent.size() / 2;
  target.resize(target_copy_size);
  size_t copied_size = mock_entry.CopyContentTo(base::span(target));
  ASSERT_EQ(copied_size, target_copy_size);
  constexpr size_t kIndexOfStart = 0;
  EXPECT_EQ(target, kContent.subspan(kIndexOfStart, target_copy_size));
}

TEST(PersistentCacheEntry, CopyContentSufficientSpace) {
  StrictMock<MockEntryImpl> mock_entry;
  EXPECT_CALL(mock_entry, GetContentSpan())
      .WillRepeatedly(Return(base::as_byte_span(kContent)));

  std::vector<uint8_t> target;

  // When the target span has sufficient capacity the entirety of the content is
  // copied.
  target.resize(kContent.size());
  ASSERT_EQ(mock_entry.CopyContentTo(base::span(target)), kContent.size());
  ASSERT_EQ(mock_entry.GetContentSpan(), target);
}

TEST(PersistentCacheEntry, CopyContentExtraSpace) {
  StrictMock<MockEntryImpl> mock_entry;
  EXPECT_CALL(mock_entry, GetContentSpan())
      .WillRepeatedly(Return(base::as_byte_span(kContent)));

  std::vector<uint8_t> target;

  // When the target span has more than sufficient capacity the entirety of the
  // content is copied.
  target.resize(kContent.size() + 10);
  ASSERT_EQ(mock_entry.CopyContentTo(base::span(target)), kContent.size());

  const base::span subspan =
      base::as_byte_span(target).subspan(size_t(0), kContent.size());
  ASSERT_EQ(mock_entry.GetContentSpan(), subspan);
}

}  // namespace persistent_cache
