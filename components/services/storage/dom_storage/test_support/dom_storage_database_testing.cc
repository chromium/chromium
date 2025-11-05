// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

// Sort by the map's session ID and storage key.
bool CompareMapMetadata(const DomStorageDatabase::MapMetadata& left,
                        const DomStorageDatabase::MapMetadata& right) {
  if (left.map_locator.session_id() == right.map_locator.session_id()) {
    return left.map_locator.storage_key() > right.map_locator.storage_key();
  }
  return left.map_locator.session_id() > right.map_locator.session_id();
}

}  // namespace

void ExpectEqualsMapLocator(const DomStorageDatabase::MapLocator& left,
                            const DomStorageDatabase::MapLocator& right) {
  EXPECT_EQ(left.session_id(), right.session_id());
  EXPECT_EQ(left.storage_key(), right.storage_key());
  EXPECT_EQ(left.map_id(), right.map_id());
}

void ExpectEqualsMapMetadata(const DomStorageDatabase::MapMetadata& left,
                             const DomStorageDatabase::MapMetadata& right) {
  ExpectEqualsMapLocator(left.map_locator, right.map_locator);
  EXPECT_EQ(left.last_accessed, right.last_accessed);
  EXPECT_EQ(left.last_modified, right.last_modified);
  EXPECT_EQ(left.total_size, right.total_size);
}

void ExpectEqualsMapMetadataSpan(
    base::span<const DomStorageDatabase::MapMetadata> left,
    base::span<const DomStorageDatabase::MapMetadata> right) {
  ASSERT_EQ(left.size(), right.size());

  // Left and right may not have the same order of elements. Create clones to
  // sort before comparing.
  std::vector<DomStorageDatabase::MapMetadata> left_clone =
      CloneMapMetadata(left);
  std::sort(left_clone.begin(), left_clone.end(), CompareMapMetadata);

  std::vector<DomStorageDatabase::MapMetadata> right_clone =
      CloneMapMetadata(right);
  std::sort(right_clone.begin(), right_clone.end(), CompareMapMetadata);

  for (size_t i = 0u; i < left.size(); ++i) {
    ExpectEqualsMapMetadata(left_clone[i], right_clone[i]);
  }
}

std::vector<DomStorageDatabase::MapMetadata> CloneMapMetadata(
    base::span<const DomStorageDatabase::MapMetadata> source_span) {
  std::vector<DomStorageDatabase::MapMetadata> results;
  for (const DomStorageDatabase::MapMetadata& source : source_span) {
    if (source.map_locator.map_id()) {
      results.push_back({
          .map_locator{
              source.map_locator.session_id(),
              source.map_locator.storage_key(),
              source.map_locator.map_id().value(),
          },
          .last_accessed{source.last_accessed},
          .last_modified{source.last_modified},
          .total_size{source.total_size},
      });
    } else {
      // `source` does not have a map ID.
      results.push_back({
          .map_locator{
              source.map_locator.session_id(),
              source.map_locator.storage_key(),
          },
          .last_accessed{source.last_accessed},
          .last_modified{source.last_modified},
          .total_size{source.total_size},
      });
    }
  }
  return results;
}

}  // namespace storage
