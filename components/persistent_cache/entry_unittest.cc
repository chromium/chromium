// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

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

}  // namespace persistent_cache
