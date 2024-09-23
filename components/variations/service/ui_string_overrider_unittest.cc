// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/variations/service/ui_string_overrider.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

constexpr size_t kNumResources = 4;

constexpr auto kResourceHashes = std::to_array<uint32_t>({
    301430091U,   // IDS_BOOKMARKS_NO_ITEMS
    2654138887U,  // IDS_BOOKMARK_BAR_IMPORT_LINK
    2894469061U,  // IDS_BOOKMARK_GROUP_FROM_IE
    3847176170U,  // IDS_BOOKMARK_GROUP_FROM_FIREFOX
});

constexpr auto kResourceIndices = std::to_array<int>({
    12500,  // IDS_BOOKMARKS_NO_ITEMS
    12501,  // IDS_BOOKMARK_BAR_IMPORT_LINK
    12502,  // IDS_BOOKMARK_GROUP_FROM_IE
    12503,  // IDS_BOOKMARK_GROUP_FROM_FIREFOX
});

}  // namespace

class UIStringOverriderTest : public ::testing::Test {
 public:
  UIStringOverriderTest() : provider_(kResourceHashes, kResourceIndices) {}

  UIStringOverriderTest(const UIStringOverriderTest&) = delete;
  UIStringOverriderTest& operator=(const UIStringOverriderTest&) = delete;

  int GetResourceIndex(uint32_t hash) {
    return provider_.GetResourceIndex(hash);
  }

 private:
  UIStringOverrider provider_;
};

TEST_F(UIStringOverriderTest, LookupNotFound) {
  EXPECT_EQ(-1, GetResourceIndex(0));
  EXPECT_EQ(-1, GetResourceIndex(kResourceHashes[kNumResources - 1] + 1));

  // Lookup a hash that shouldn't exist.
  // 3847176171U is 1 + the hash for IDS_BOOKMARK_GROUP_FROM_FIREFOX.
  EXPECT_EQ(-1, GetResourceIndex(3847176171U));
}

TEST_F(UIStringOverriderTest, LookupFound) {
  for (size_t i = 0; i < kNumResources; ++i)
    EXPECT_EQ(kResourceIndices[i], GetResourceIndex(kResourceHashes[i]));
}

}  // namespace variations
