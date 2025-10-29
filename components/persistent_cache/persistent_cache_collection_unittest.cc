// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache_collection.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/sqlite/constants.h"
#include "components/persistent_cache/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<base::FilePath> GetPathsInDir(const base::FilePath& directory) {
  std::vector<base::FilePath> paths;
  base::FileEnumerator(directory, /*recursive=*/false,
                       base::FileEnumerator::FILES)
      .ForEach([&paths](const base::FilePath& file_path) {
        paths.push_back(file_path);
      });
  return paths;
}

}  // namespace

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
  EXPECT_THAT(collection.Insert(cache_id, key, base::as_byte_span(key)),
              base::test::HasValue());
  ASSERT_THAT(collection.Find(cache_id, key),
              base::test::ValueIs(HasContents(base::as_byte_span(key))));
}

TEST_F(PersistentCacheCollectionTest, DeleteAllFiles) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kTargetFootprint);

  std::string cache_id("cache_id");
  std::string key("key");
  EXPECT_THAT(collection.Insert(cache_id, key, base::as_byte_span(key)),
              base::test::HasValue());

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
  EXPECT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ValueIs(testing::IsNull()));
  EXPECT_THAT(collection.Find(first_cache_id, second_key),
              base::test::ValueIs(testing::IsNull()));
  EXPECT_THAT(collection.Find(second_cache_id, first_key),
              base::test::ValueIs(testing::IsNull()));
  EXPECT_THAT(collection.Find(second_cache_id, second_key),
              base::test::ValueIs(testing::IsNull()));

  // Inserting for a certain cache id allows retrieval for this id and this id
  // only.
  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              base::test::HasValue());
  ASSERT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ValueIs(
                  HasContents(base::byte_span_from_cstring(first_content))));

  EXPECT_THAT(collection.Find(second_cache_id, first_key),
              base::test::ValueIs(testing::IsNull()));
}

TEST_F(PersistentCacheCollectionTest, RetrievalAfterClear) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  // Test basic retrieval.
  EXPECT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ValueIs(testing::IsNull()));

  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              base::test::HasValue());

  EXPECT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ValueIs(testing::NotNull()));

  collection.ClearForTesting();

  // Retrieval still works after clear because data persistence is unaffected by
  // lifetime of PersistentCache instances.
  EXPECT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ValueIs(testing::NotNull()));
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

      EXPECT_THAT(collection.Insert(number, number, base::as_byte_span(number)),
                  base::test::HasValue());

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
    EXPECT_THAT(collection.Find(number, number),
                base::test::ValueIs(testing::NotNull()));
  }

  // Add one more item which should bring things over the limit.
  std::string number = base::NumberToString(i + 1);
  EXPECT_THAT(collection.Insert(number, number, base::as_byte_span(number)),
              base::test::HasValue());

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
  EXPECT_THAT(
      collection.Insert(all_chars_key, number, base::as_byte_span(number)),
      base::test::HasValue());
  EXPECT_THAT(collection.Find(all_chars_key, number),
              base::test::ValueIs(testing::NotNull()));
}

TEST_F(PersistentCacheCollectionTest, InstancesAbandonnedOnLRUEviction) {
  static constexpr char kKey[] = "KEY";
  static constexpr size_t kLruCacheCapacity = 5;
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB,
                                       kLruCacheCapacity);

  std::vector<std::unique_ptr<PersistentCache>> caches;
  size_t cache_id = 0;
  auto get_increasing_cache_id = [&cache_id]() {
    return base::NumberToString(cache_id++);
  };

  // Creates caches exactly up to capacity.
  for (size_t i = 0; i < kLruCacheCapacity; ++i) {
    ASSERT_OK_AND_ASSIGN(auto params, collection.ExportReadWriteBackendParams(
                                          get_increasing_cache_id()));
    caches.push_back(PersistentCache::Open(std::move(params)));
  }
  ASSERT_NE(caches.front(), nullptr);

  // Find succeeds since the instance is not evicted yet.
  EXPECT_THAT(caches.front()->Find(kKey), base::test::HasValue());

  // Create one more cache which goes over the limit.
  ASSERT_OK_AND_ASSIGN(auto params, collection.ExportReadWriteBackendParams(
                                        get_increasing_cache_id()));
  caches.emplace_back(PersistentCache::Open(std::move(params)));

  // The first cache has now been evicted and is abandoned.
  EXPECT_THAT(caches.front()->Find(kKey),
              base::test::ErrorIs(TransactionError::kConnectionError));
}

TEST_F(PersistentCacheCollectionTest, InstancesAbandonnedOnClear) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string key("key");
  ASSERT_OK_AND_ASSIGN(auto params,
                       collection.ExportReadWriteBackendParams(key));
  auto cache = PersistentCache::Open(std::move(params));

  collection.ClearForTesting();
  EXPECT_THAT(cache->Find(key),
              base::test::ErrorIs(TransactionError::kConnectionError));
}

TEST_F(PersistentCacheCollectionTest, AbandonnedErrorsDoNotCauseDeletions) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              base::test::HasValue());
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      testing::UnorderedElementsAre(
          testing::Property(&base::FilePath::Extension,
                            testing::StrEq(sqlite::kDbFileExtension)),
          testing::Property(&base::FilePath::Extension,
                            testing::StrEq(sqlite::kJournalFileExtension))));

  ASSERT_OK_AND_ASSIGN(auto params,
                       collection.ExportReadWriteBackendParams(first_cache_id));
  auto cache = PersistentCache::Open(std::move(params));
  cache->Abandon();

  EXPECT_THAT(cache->Find(first_key),
              base::test::ErrorIs(TransactionError::kConnectionError));
  EXPECT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ErrorIs(TransactionError::kConnectionError));

  // Files are still there.
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      testing::UnorderedElementsAre(
          testing::Property(&base::FilePath::Extension,
                            testing::StrEq(sqlite::kDbFileExtension)),
          testing::Property(&base::FilePath::Extension,
                            testing::StrEq(sqlite::kJournalFileExtension))));
}

TEST_F(PersistentCacheCollectionTest, PermanentErrorCausesDeletion) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  static constexpr char first_content[] = "first_content";

  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              base::test::HasValue());
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      testing::UnorderedElementsAre(
          testing::Property(&base::FilePath::Extension,
                            testing::StrEq(sqlite::kDbFileExtension)),
          testing::Property(&base::FilePath::Extension,
                            testing::StrEq(sqlite::kJournalFileExtension))));

  // TODO(https://crbug.com/377475540): Instead of triggering an error in a
  // backend specific way PersistentCacheCollection should have a way to inject
  // mock BackendStorage::Delegate.
  base::FileEnumerator(temp_dir_.GetPath(), /*recursive=*/false,
                       base::FileEnumerator::FILES)
      .ForEach([](const base::FilePath& file_path) {
        base::File file;
        file.Initialize(file_path, base::File::FLAG_WRITE |
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_CAN_DELETE_ON_CLOSE |
                                       base::File::FLAG_WIN_SHARE_DELETE);

        // Truncate which will cause future requests to start failing.
        CHECK(file.IsValid());
        file.SetLength(0);
      });

  // Permanent error because there are no more valid files.
  EXPECT_THAT(collection.Find(first_cache_id, first_key),
              base::test::ErrorIs(TransactionError::kPermanent));

  // TODO(https://crbug.com/377475540): As in previous item once we use mocking
  // to trigger failures we should validate that transient errors are handled
  // properly in a backend agnostic way.

  // Files got deleted on permanent error.
  EXPECT_THAT(GetPathsInDir(temp_dir_.GetPath()), testing::IsEmpty());
}

using PersistentCacheCollectionDeathTest = PersistentCacheCollectionTest;

// Tests that trying to operate on a cache in a collection crashes if an
// invalid cache_id is used.
TEST_F(PersistentCacheCollectionDeathTest, BadKeysCrash) {
  EXPECT_CHECK_DEATH({
    // There is no expectation for the return value we can test since death is
    // expected
    std::ignore = PersistentCacheCollection(temp_dir_.GetPath(), kOneHundredMiB)
                      .Insert(std::string("BADKEY"), "key",
                              base::byte_span_from_cstring("value"));
  });
}

}  // namespace persistent_cache
