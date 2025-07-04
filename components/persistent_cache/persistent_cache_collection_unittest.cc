// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache_collection.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "components/persistent_cache/backend_params_manager.h"
#include "components/persistent_cache/entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {
namespace {

// Default value large enough to no interfere with functioning of tests.
constexpr size_t kTargetFootprint = 1024 * 1024 * 100;

TEST(PersistentCacheCollection, Retrieval) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()),
      kTargetFootprint);

  constexpr char first_cache_id[] = "first_cache_id";
  constexpr char second_cache_id[] = "second_cache_id";

  constexpr char first_key[] = "first_key";
  constexpr char second_key[] = "second_key";

  constexpr const char first_content[] = "first_content";

  // At first there is nothing in the collection.
  EXPECT_EQ(collection.Find(first_cache_id, first_key), nullptr);
  EXPECT_EQ(collection.Find(first_cache_id, second_key), nullptr);
  EXPECT_EQ(collection.Find(second_cache_id, first_key), nullptr);
  EXPECT_EQ(collection.Find(second_cache_id, second_key), nullptr);

  // Inserting for a certain cache id allows retrieval for this id and this id
  // only.
  collection.Insert(first_cache_id, first_key,
                    base::byte_span_from_cstring(first_content));
  auto entry = collection.Find(first_cache_id, first_key);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetContentSpan(),
            base::byte_span_from_cstring(first_content));
  EXPECT_EQ(collection.Find(second_cache_id, first_key), nullptr);
}

TEST(PersistentCacheCollection, RetrievalAfterClear) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()),
      kTargetFootprint);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  // Test basic retrieval.
  EXPECT_EQ(collection.Find(first_cache_id, first_key), nullptr);
  collection.Insert(first_cache_id, first_key,
                    base::byte_span_from_cstring(first_content));
  EXPECT_NE(collection.Find(first_cache_id, first_key), nullptr);

  // Retrieval still works after clear because data persistence is unnafected by
  // lifetime of PersistentCache instances.
  collection.ClearForTesting();
  EXPECT_NE(collection.Find(first_cache_id, first_key), nullptr);
}

TEST(PersistentCacheCollection, DeleteAllFiles) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()),
      kTargetFootprint);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  // Inserting an entry makes it available.
  collection.Insert(first_cache_id, first_key,
                    base::byte_span_from_cstring(first_content));
  EXPECT_NE(collection.Find(first_cache_id, first_key), nullptr);

  collection.DeleteAllFiles();

  // After deletion the content is not available anymore.
  EXPECT_EQ(collection.Find(first_cache_id, first_key), nullptr);
}

TEST(PersistentCacheCollection, ContinuousFootPrintReduction) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  constexpr int64_t kSmallFootprint = 128;

  PersistentCacheCollection collection(
      std::make_unique<BackendParamsManager>(temp_dir.GetPath()),
      kSmallFootprint);

  int i = 0;
  int64_t added_footprint = 0;

  // Add things right up to the limit where files start to be deleted.
  while (added_footprint < kSmallFootprint) {
    std::string number = base::NumberToString(i);

    // Account for size of key and value.
    int64_t footprint_after_insertion = added_footprint + number.length() * 2;

    if (footprint_after_insertion < kSmallFootprint) {
      int64_t directory_size_before =
          base::ComputeDirectorySize(temp_dir.GetPath());

      collection.Insert(number, number, base::as_byte_span(number));

      int64_t directory_size_after =
          base::ComputeDirectorySize(temp_dir.GetPath());

      // If there's no footprint reduction and the new values are being stored
      // then directory size is just going up.
      EXPECT_GT(directory_size_after, directory_size_before);
    }

    added_footprint = footprint_after_insertion;
    ++i;
  }

  // If `kSmallFootprint` is not large enough to trigger at least two successful
  // insertions into the cache the test does not provide sufficient coverage.
  ASSERT_GT(i, 2);

  int64_t directory_size_before =
      base::ComputeDirectorySize(temp_dir.GetPath());

  // Since no footprint reduction should have been triggered all values added
  // should still be available.
  for (int j = 0; j < i - 1; ++j) {
    std::string number = base::NumberToString(j);
    EXPECT_NE(collection.Find(number, number), nullptr);
  }

  // Add one more item which should bring things over the limit.
  std::string number = base::NumberToString(i + 1);
  collection.Insert(number, number, base::as_byte_span(number));

  int64_t directory_size_after = base::ComputeDirectorySize(temp_dir.GetPath());

  // Footprint reduction happened automatically. Note that's it's not possible
  // to specifically know what the current footprint is since the last insert
  // took place after the footprint reduction.
  EXPECT_LT(directory_size_after, directory_size_before);
}

}  // namespace
}  // namespace persistent_cache
