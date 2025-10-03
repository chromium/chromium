// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache_collection.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

class PersistentCacheCollectionTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  static constexpr int64_t kTargetFootprint = 20000;
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Two operations with the same cache_id operate on the same database.
TEST_F(PersistentCacheCollectionTest, CreateAndUse) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kTargetFootprint);

  std::string cache_id("cache_id");
  std::string key("key");
  collection.Insert(cache_id, key, base::as_byte_span(key));
  auto entry = collection.Find(cache_id, key);
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->GetContentSpan(), base::as_byte_span(key));
}

TEST_F(PersistentCacheCollectionTest, DeleteAllFiles) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kTargetFootprint);

  std::string cache_id("cache_id");
  std::string key("key");
  collection.Insert(cache_id, key, base::as_byte_span(key));

  // Inserting an entry should have created at least one file.
  EXPECT_FALSE(base::IsDirectoryEmpty(temp_dir_.GetPath()));

  collection.DeleteAllFiles();
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

static constexpr int64_t kOneHundredMiB = 100 * 1024 * 1024;

TEST_F(PersistentCacheCollectionTest, Retrieval) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

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

TEST_F(PersistentCacheCollectionTest, RetrievalAfterClear) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

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

TEST_F(PersistentCacheCollectionTest, ContinuousFootPrintReduction) {
  constexpr int64_t kSmallFootprint = 128;

  PersistentCacheCollection collection(temp_dir_.GetPath(), kSmallFootprint);

  int i = 0;
  int64_t added_footprint = 0;

  // Add things right up to the limit where files start to be deleted.
  while (added_footprint < kSmallFootprint) {
    std::string number = base::NumberToString(i);

    // Account for size of key and value.
    int64_t footprint_after_insertion = added_footprint + (number.length() * 2);

    if (footprint_after_insertion < kSmallFootprint) {
      int64_t directory_size_before =
          base::ComputeDirectorySize(temp_dir_.GetPath());

      collection.Insert(number, number, base::as_byte_span(number));

      int64_t directory_size_after =
          base::ComputeDirectorySize(temp_dir_.GetPath());

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
      base::ComputeDirectorySize(temp_dir_.GetPath());

  // Since no footprint reduction should have been triggered all values added
  // should still be available.
  for (int j = 0; j < i - 1; ++j) {
    std::string number = base::NumberToString(j);
    EXPECT_NE(collection.Find(number, number), nullptr);
  }

  // Add one more item which should bring things over the limit.
  std::string number = base::NumberToString(i + 1);
  collection.Insert(number, number, base::as_byte_span(number));

  int64_t directory_size_after =
      base::ComputeDirectorySize(temp_dir_.GetPath());

  // Footprint reduction happened automatically. Note that's it's not possible
  // to specifically know what the current footprint is since the last insert
  // took place after the footprint reduction.
  EXPECT_LT(directory_size_after, directory_size_before);
}

TEST_F(PersistentCacheCollectionTest, BaseNameFromCacheId) {
  // Invalid tokens results in empty string and not a crash.
  EXPECT_EQ(PersistentCacheCollection::BaseNameFromCacheId("`"),
            base::FilePath());
  EXPECT_EQ(PersistentCacheCollection::BaseNameFromCacheId("``"),
            base::FilePath());

  // Verify file name is obfuscated.
  std::string cache_id("devs_first_db");
  base::FilePath base_name(
      PersistentCacheCollection::BaseNameFromCacheId(cache_id));
  EXPECT_EQ(base_name.value().find(base::FilePath::FromASCII(cache_id).value()),
            std::string::npos);
  EXPECT_EQ(base_name.value().find(FILE_PATH_LITERAL("devs")),
            std::string::npos);
  EXPECT_EQ(base_name.value().find(FILE_PATH_LITERAL("first")),
            std::string::npos);
}

TEST_F(PersistentCacheCollectionTest, FullAllowedCharacterSetHandled) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string all_chars_key =
      PersistentCacheCollection::GetAllAllowedCharactersInCacheIds();
  std::string number("number");
  collection.Insert(all_chars_key, number, base::as_byte_span(number));
  ASSERT_NE(collection.Find(all_chars_key, number), nullptr);
}

using PersistentCacheCollectionDeathTest = PersistentCacheCollectionTest;

// Tests that trying to operate on a cache in a collection crashes if an
// invalid cache_id is used.
TEST_F(PersistentCacheCollectionDeathTest, BadKeysCrash) {
  EXPECT_CHECK_DEATH({
    PersistentCacheCollection(temp_dir_.GetPath(), kOneHundredMiB)
        .Insert(std::string("BADKEY"), "key",
                base::byte_span_from_cstring("value"));
  });
}

}  // namespace persistent_cache
