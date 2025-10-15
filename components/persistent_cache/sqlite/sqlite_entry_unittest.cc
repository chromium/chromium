// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>

#include "base/containers/span.h"
#include "components/persistent_cache/sqlite/sqlite_entry_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kContent[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

}  // namespace

namespace persistent_cache {

TEST(SqliteEntryTest, ConstructionTakesOwnershipOfValue) {
  std::string copy = kContent;
  SqliteEntryImpl sql_entry(std::move(copy), EntryMetadata{});
  // Move took place.
  EXPECT_TRUE(copy.empty());
  EXPECT_EQ(sql_entry.GetContentSpan(), base::span_from_cstring(kContent));
  EXPECT_EQ(sql_entry.GetContentSize(), std::strlen(kContent));
}

TEST(SqliteEntryTest, ConstructionFromEmptyValueLeadsToEmptyEntry) {
  SqliteEntryImpl sql_entry(std::string(""), EntryMetadata{});
  EXPECT_TRUE(sql_entry.GetContentSpan().empty());
  EXPECT_EQ(sql_entry.GetContentSize(), 0ull);
}

}  // namespace persistent_cache
