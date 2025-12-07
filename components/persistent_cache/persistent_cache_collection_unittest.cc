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
#include "components/persistent_cache/mock/mock_backend.h"
#include "components/persistent_cache/mock/mock_backend_storage_delegate.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/sqlite/constants.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"
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

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using testing::_;
using testing::AnyNumber;
using testing::Eq;
using testing::IsEmpty;
using testing::IsTrue;
using testing::Ne;
using testing::Optional;
using testing::Property;
using testing::ResultOf;
using testing::Return;
using testing::StrEq;
using testing::UnorderedElementsAre;

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
              HasValue());
  ASSERT_THAT(FindEntry(collection, cache_id, key),
              ValueIs(Optional(ContentEq(base::as_byte_span(key)))));
}

TEST_F(PersistentCacheCollectionTest, DeleteAllFiles) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kTargetFootprint);

  std::string cache_id("cache_id");
  std::string key("key");
  EXPECT_THAT(collection.Insert(cache_id, key, base::as_byte_span(key)),
              HasValue());

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
  EXPECT_THAT(FindEntry(collection, first_cache_id, first_key),
              ValueIs(Eq(std::nullopt)));
  EXPECT_THAT(FindEntry(collection, first_cache_id, second_key),
              ValueIs(Eq(std::nullopt)));
  EXPECT_THAT(FindEntry(collection, second_cache_id, first_key),
              ValueIs(Eq(std::nullopt)));
  EXPECT_THAT(FindEntry(collection, second_cache_id, second_key),
              ValueIs(Eq(std::nullopt)));

  // Inserting for a certain cache id allows retrieval for this id and this id
  // only.
  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              HasValue());
  ASSERT_THAT(FindEntry(collection, first_cache_id, first_key),
              ValueIs(Optional(
                  ContentEq(base::byte_span_from_cstring(first_content)))));

  EXPECT_THAT(FindEntry(collection, second_cache_id, first_key),
              ValueIs(Eq(std::nullopt)));
}

TEST_F(PersistentCacheCollectionTest, RetrievalAfterClear) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  // Test basic retrieval.
  EXPECT_THAT(FindEntry(collection, first_cache_id, first_key),
              ValueIs(Eq(std::nullopt)));

  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              HasValue());

  EXPECT_THAT(FindEntry(collection, first_cache_id, first_key),
              ValueIs(Ne(std::nullopt)));

  collection.Clear();

  // Retrieval still works after clear because data persistence is unaffected by
  // lifetime of PersistentCache instances.
  EXPECT_THAT(FindEntry(collection, first_cache_id, first_key),
              ValueIs(Ne(std::nullopt)));
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
                  HasValue());

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
    EXPECT_THAT(FindEntry(collection, number, number),
                ValueIs(Ne(std::nullopt)));
  }

  // Add one more item which should bring things over the limit.
  std::string number = base::NumberToString(i + 1);
  EXPECT_THAT(collection.Insert(number, number, base::as_byte_span(number)),
              HasValue());

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
      HasValue());
  EXPECT_THAT(FindEntry(collection, all_chars_key, number),
              ValueIs(Ne(std::nullopt)));
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
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        collection.ShareReadWriteConnection(get_increasing_cache_id()));
    caches.push_back(PersistentCache::Bind(std::move(pending_backend)));
  }
  ASSERT_NE(caches.front(), nullptr);

  // Find succeeds since the instance is not evicted yet.
  EXPECT_THAT(FindEntry(*caches.front(), kKey), HasValue());

  // Create one more cache which goes over the limit.
  ASSERT_OK_AND_ASSIGN(
      auto pending_backend,
      collection.ShareReadWriteConnection(get_increasing_cache_id()));
  caches.emplace_back(PersistentCache::Bind(std::move(pending_backend)));

  // The first cache has now been evicted and is abandoned.
  EXPECT_THAT(FindEntry(*caches.front(), kKey),
              ErrorIs(TransactionError::kConnectionError));
}

TEST_F(PersistentCacheCollectionTest, InstancesAbandonnedOnClear) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string key("key");
  ASSERT_OK_AND_ASSIGN(auto pending_backend,
                       collection.ShareReadWriteConnection(key));
  auto cache = PersistentCache::Bind(std::move(pending_backend));

  collection.Clear();
  EXPECT_THAT(FindEntry(*cache, key),
              ErrorIs(TransactionError::kConnectionError));
}

TEST_F(PersistentCacheCollectionTest, AbandonnedErrorsDoNotCauseDeletions) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  constexpr const char first_content[] = "first_content";

  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              HasValue());
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      UnorderedElementsAre(
          Property(&base::FilePath::Extension, StrEq(sqlite::kDbFileExtension)),
          Property(&base::FilePath::Extension,
                   StrEq(sqlite::kJournalFileExtension))));

  ASSERT_OK_AND_ASSIGN(auto pending_backend,
                       collection.ShareReadWriteConnection(first_cache_id));
  auto cache = PersistentCache::Bind(std::move(pending_backend));
  EXPECT_EQ(cache->Abandon(), LockState::kNotHeld);

  EXPECT_THAT(FindEntry(*cache, first_key),
              ErrorIs(TransactionError::kConnectionError));
  EXPECT_THAT(FindEntry(collection, first_cache_id, first_key),
              ErrorIs(TransactionError::kConnectionError));

  // Files are still there.
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      UnorderedElementsAre(
          Property(&base::FilePath::Extension, StrEq(sqlite::kDbFileExtension)),
          Property(&base::FilePath::Extension,
                   StrEq(sqlite::kJournalFileExtension))));
}

TEST_F(PersistentCacheCollectionTest, EvictWhileLockedDeletesFiles) {
  auto mock_delegate =
      std::make_unique<testing::NiceMock<MockBackendStorageDelegate>>();
  auto backend = std::make_unique<testing::NiceMock<MockBackend>>();

  // Backend default behavior.
  ON_CALL(*backend, IsReadOnly()).WillByDefault(Return(false));

  // Simulates the fact that readers are left over on abandonment.
  EXPECT_CALL(*backend, Abandon()).WillOnce(Return(LockState::kReading));

  // Return the mock backend from the BackendStorage::Delegate when requested,
  // and remember the cache base name.
  base::FilePath saved_base_name;
  EXPECT_CALL(*mock_delegate, MakeBackend(temp_dir_.GetPath(), _, false, false))
      .WillOnce([&](const base::FilePath& directory,
                    const base::FilePath& base_name, bool single_connection,
                    bool journal_mode_wal) {
        saved_base_name = base_name;
        return (std::move(backend));
      });

  // This call only takes place as a reaction to the reader being left over
  // after abandonment.
  EXPECT_CALL(
      *mock_delegate,
      DeleteFiles(temp_dir_.GetPath(),
                  ResultOf(
                      [&saved_base_name](const base::FilePath& base_name) {
                        return base_name == saved_base_name;
                      },
                      IsTrue())));

  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB,
                                       std::move(mock_delegate));
  std::string first_cache_id = "first_cache_id";
  // `ExportReadWriteBackendParams` called to force the collection to create a
  // `PersistentCache`.
  collection.ShareReadWriteConnection(first_cache_id);
  collection.Clear();
}

TEST_F(PersistentCacheCollectionTest,
       BackendStorageCreationAfterDeleteSucceedsWithHeldFiles) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);
  std::string first_cache_id = "first_cache_id";

  // Files exists after creating a params since `ExportReadWriteBackendParams`
  // forces the creation of a `PersistentCache`.
  ASSERT_THAT(collection.ShareReadWriteConnection(first_cache_id),
              Ne(std::nullopt));
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      UnorderedElementsAre(
          Property(&base::FilePath::Extension, StrEq(sqlite::kDbFileExtension)),
          Property(&base::FilePath::Extension,
                   StrEq(sqlite::kJournalFileExtension))));

  // No more files after delete.
  collection.DeleteAllFiles();
  EXPECT_THAT(GetPathsInDir(temp_dir_.GetPath()), IsEmpty());

  // It's possible to recreate params/files with the same cache_id.
  ASSERT_OK_AND_ASSIGN(auto other_pending_backend,
                       collection.ShareReadWriteConnection(first_cache_id));
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      UnorderedElementsAre(
          Property(&base::FilePath::Extension, StrEq(sqlite::kDbFileExtension)),
          Property(&base::FilePath::Extension,
                   StrEq(sqlite::kJournalFileExtension))));
}

TEST_F(PersistentCacheCollectionTest, PermanentErrorCausesDeletion) {
  PersistentCacheCollection collection(temp_dir_.GetPath(), kOneHundredMiB);

  std::string first_cache_id = "first_cache_id";
  std::string first_key = "first_key";
  static constexpr char first_content[] = "first_content";

  EXPECT_THAT(collection.Insert(first_cache_id, first_key,
                                base::byte_span_from_cstring(first_content)),
              HasValue());
  EXPECT_THAT(
      GetPathsInDir(temp_dir_.GetPath()),
      UnorderedElementsAre(
          Property(&base::FilePath::Extension, StrEq(sqlite::kDbFileExtension)),
          Property(&base::FilePath::Extension,
                   StrEq(sqlite::kJournalFileExtension))));

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
  EXPECT_THAT(FindEntry(collection, first_cache_id, first_key),
              ErrorIs(TransactionError::kPermanent));

  // TODO(https://crbug.com/377475540): As in previous item once we use mocking
  // to trigger failures we should validate that transient errors are handled
  // properly in a backend agnostic way.

  // Files got deleted on permanent error.
  EXPECT_THAT(GetPathsInDir(temp_dir_.GetPath()), IsEmpty());
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
