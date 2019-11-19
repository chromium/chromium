// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/cache/image_store_types.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;
using testing::_;

namespace image_fetcher {

namespace {

constexpr char kImageKey[] = "http://cat.org/cat.jpg";
constexpr char kOtherImageKey[] = "http://cat.org/dog.jpg";
constexpr int kImageDataLength = 5;

}  // namespace

class CachedImageFetcherImageMetadataStoreLevelDBTest : public testing::Test {
 public:
  CachedImageFetcherImageMetadataStoreLevelDBTest() : db_(nullptr) {}

  void CreateDatabase() {
    // Reset everything.
    db_ = nullptr;
    clock_ = nullptr;
    metadata_store_.reset();

    // Setup the clock.
    clock_ = std::make_unique<base::SimpleTestClock>();
    clock_->SetNow(base::Time());

    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<CachedImageMetadataProto>>(&db_store_);
    db_ = db.get();
    metadata_store_ = std::make_unique<ImageMetadataStoreLevelDB>(std::move(db),
                                                                  clock_.get());
  }

  void InitializeDatabase() {
    EXPECT_CALL(*this, OnInitialized());
    metadata_store()->Initialize(base::BindOnce(
        &CachedImageFetcherImageMetadataStoreLevelDBTest::OnInitialized,
        base::Unretained(this)));
    db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

    RunUntilIdle();
  }

  void PrepareDatabase(bool initialize) {
    CreateDatabase();
    InitializeDatabase();
    metadata_store()->SaveImageMetadata(kImageKey, kImageDataLength,
                                        /* needs_transcoding */ false);
    ASSERT_TRUE(IsDataPresent(kImageKey));

    if (!initialize) {
      CreateDatabase();
    }
  }

  void RunGarbageCollection(base::TimeDelta move_clock_forward,
                            base::TimeDelta time_alive,
                            KeysCallback callback) {
    RunGarbageCollection(move_clock_forward, time_alive, std::move(callback),
                         false, 0);
  }

  void RunGarbageCollection(base::TimeDelta move_clock_forward_time,
                            base::TimeDelta look_back_time,
                            KeysCallback callback,
                            bool specify_bytes,
                            size_t bytes_left) {
    clock()->SetNow(clock()->Now() + move_clock_forward_time);

    if (specify_bytes) {
      metadata_store()->EvictImageMetadata(clock()->Now() - look_back_time,
                                           bytes_left, std::move(callback));
    } else {
      metadata_store()->EvictImageMetadata(clock()->Now() - look_back_time,
                                           std::move(callback));
    }
  }

  // Returns true if the data is present for the given key.
  bool IsDataPresent(const std::string& key) {
    return db_store_.find(key) != db_store_.end();
  }

  void AssertDataPresent(const std::string& key,
                         int64_t data_size,
                         base::Time creation_time,
                         base::Time last_used_time,
                         bool needs_transcoding) {
    if (!IsDataPresent(key)) {
      ASSERT_TRUE(false);
    }

    auto entry = db_store_[key];
    ASSERT_EQ(entry.data_size(), data_size);
    ASSERT_EQ(entry.creation_time(),
              creation_time.since_origin().InMicroseconds());
    ASSERT_EQ(entry.last_used_time(),
              last_used_time.since_origin().InMicroseconds());
    ASSERT_EQ(entry.needs_transcoding(), needs_transcoding);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  base::SimpleTestClock* clock() { return clock_.get(); }
  ImageMetadataStore* metadata_store() { return metadata_store_.get(); }
  FakeDB<CachedImageMetadataProto>* db() { return db_; }

  MOCK_METHOD0(OnInitialized, void());
  MOCK_METHOD1(OnKeysReturned, void(std::vector<std::string>));
  MOCK_METHOD1(OnStoreOperationComplete, void(bool));
  MOCK_METHOD1(OnImageMetadataLoaded,
               void(base::Optional<CachedImageMetadataProto>));

 private:
  std::unique_ptr<base::SimpleTestClock> clock_;
  FakeDB<CachedImageMetadataProto>* db_;
  std::map<std::string, CachedImageMetadataProto> db_store_;
  std::unique_ptr<ImageMetadataStoreLevelDB> metadata_store_;

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherImageMetadataStoreLevelDBTest);
};

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, Initialize) {
  CreateDatabase();

  EXPECT_FALSE(metadata_store()->IsInitialized());
  InitializeDatabase();
  EXPECT_TRUE(metadata_store()->IsInitialized());
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, SaveBeforeInit) {
  CreateDatabase();
  EXPECT_FALSE(metadata_store()->IsInitialized());
  // Start an image load before the database is initialized.
  metadata_store()->SaveImageMetadata(kImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);

  InitializeDatabase();
  EXPECT_TRUE(metadata_store()->IsInitialized());

  ASSERT_FALSE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, Save) {
  CreateDatabase();
  InitializeDatabase();

  metadata_store()->SaveImageMetadata(kImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);
  AssertDataPresent(kImageKey, kImageDataLength, clock()->Now(), clock()->Now(),
                    /* needs_transcoding */ false);
  metadata_store()->SaveImageMetadata(kImageKey, kImageDataLength,
                                      /* needs_transcoding */ true);
  AssertDataPresent(kImageKey, kImageDataLength, clock()->Now(), clock()->Now(),
                    /* needs_transcoding */ true);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, DeleteBeforeInit) {
  PrepareDatabase(false);
  metadata_store()->DeleteImageMetadata(kImageKey);

  InitializeDatabase();
  ASSERT_TRUE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, Delete) {
  // Put some data in the database to start.
  CreateDatabase();
  InitializeDatabase();
  metadata_store()->SaveImageMetadata(kImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);
  ASSERT_TRUE(IsDataPresent(kImageKey));

  // Delete the data.
  metadata_store()->DeleteImageMetadata(kImageKey);
  RunUntilIdle();

  ASSERT_FALSE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, DeleteDifferentKey) {
  // Put some data in the database to start.
  CreateDatabase();
  InitializeDatabase();
  metadata_store()->SaveImageMetadata(kImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);
  ASSERT_TRUE(IsDataPresent(kImageKey));

  // Delete the data.
  metadata_store()->DeleteImageMetadata(kOtherImageKey);
  RunUntilIdle();

  ASSERT_TRUE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       UpdateImageMetadataBeforeInit) {
  PrepareDatabase(false);

  // Call should be ignored because the store isn't initialized.
  metadata_store()->UpdateImageMetadata(kImageKey);
  RunUntilIdle();

  InitializeDatabase();
  AssertDataPresent(kImageKey, kImageDataLength, clock()->Now(), clock()->Now(),
                    /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, UpdateImageMetadata) {
  PrepareDatabase(true);

  clock()->SetNow(base::Time() + base::TimeDelta::FromHours(1));
  metadata_store()->UpdateImageMetadata(kImageKey);
  db()->LoadCallback(true);
  db()->UpdateCallback(true);
  RunUntilIdle();

  AssertDataPresent(kImageKey, kImageDataLength,
                    clock()->Now() - base::TimeDelta::FromHours(1),
                    clock()->Now(), /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       UpdateImageMetadataNoHits) {
  PrepareDatabase(true);

  metadata_store()->UpdateImageMetadata(kOtherImageKey);
  db()->LoadCallback(true);
  db()->UpdateCallback(true);
  RunUntilIdle();

  AssertDataPresent(kImageKey, kImageDataLength, clock()->Now(), clock()->Now(),
                    /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       UpdateImageMetadataLoadFailed) {
  PrepareDatabase(true);

  metadata_store()->UpdateImageMetadata(kOtherImageKey);
  db()->LoadCallback(true);
  RunUntilIdle();

  AssertDataPresent(kImageKey, kImageDataLength, clock()->Now(), clock()->Now(),
                    /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, GetAllKeysBeforeInit) {
  PrepareDatabase(false);

  // A GC call before the db is initialized should be ignore.
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>()));
  metadata_store()->GetAllKeys(base::BindOnce(
      &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
      base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, GetAllKeys) {
  PrepareDatabase(true);
  metadata_store()->SaveImageMetadata(kOtherImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);

  // A GC call before the db is initialized should be ignore.
  EXPECT_CALL(
      *this,
      OnKeysReturned(std::vector<std::string>({kImageKey, kOtherImageKey})));
  metadata_store()->GetAllKeys(base::BindOnce(
      &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
      base::Unretained(this)));
  db()->LoadKeysCallback(true);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, GetAllKeysLoadFailed) {
  PrepareDatabase(true);
  metadata_store()->SaveImageMetadata(kOtherImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);

  // A GC call before the db is initialized should be ignore.
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>({})));
  metadata_store()->GetAllKeys(base::BindOnce(
      &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
      base::Unretained(this)));
  db()->LoadKeysCallback(false);
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, GetEstimatedSize) {
  PrepareDatabase(true);

  EXPECT_EQ(5, metadata_store()->GetEstimatedSize());
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       GarbageCollectBeforeInit) {
  PrepareDatabase(false);

  // A GC call before the db is initialized should be ignore.
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>()));
  RunGarbageCollection(
      base::TimeDelta::FromHours(1), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)),
      true, 0);
  RunUntilIdle();
  ASSERT_TRUE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, GarbageCollect) {
  PrepareDatabase(true);

  // Calling GC with something to be collected.
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>({kImageKey})));
  RunGarbageCollection(
      base::TimeDelta::FromHours(1), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)));
  db()->LoadCallback(true);
  db()->UpdateCallback(true);

  ASSERT_FALSE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, GarbageCollectNoHits) {
  PrepareDatabase(true);
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>()));

  // Run GC without moving the clock forward, should result in no hits.
  RunGarbageCollection(
      base::TimeDelta::FromHours(0), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)));
  db()->LoadCallback(true);
  db()->UpdateCallback(true);

  ASSERT_TRUE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       GarbageCollectWithBytesProvided) {
  PrepareDatabase(true);

  // Insert an item one our later.
  clock()->SetNow(clock()->Now() + base::TimeDelta::FromHours(1));
  metadata_store()->SaveImageMetadata(kOtherImageKey, kImageDataLength,
                                      /* needs_transcoding */ false);
  clock()->SetNow(clock()->Now() - base::TimeDelta::FromHours(1));
  ASSERT_TRUE(IsDataPresent(kOtherImageKey));

  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>({kImageKey})));

  // Run GC without moving the clock forward, should result in no hits.
  // Byte limit set so the one garbage collected should be enough. The older
  // entry should be gc'd kImageKey, the other should stay kOtherImageKey.
  RunGarbageCollection(
      base::TimeDelta::FromHours(1), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)),
      true, 5);
  db()->LoadCallback(true);
  db()->UpdateCallback(true);

  ASSERT_FALSE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       GarbageCollectNoHitsButBytesProvided) {
  PrepareDatabase(true);
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>({kImageKey})));

  // Run GC without moving the clock forward, should result in no hits.
  // Run GC with a byte limit of 0, everything should go.
  RunGarbageCollection(
      base::TimeDelta::FromHours(0), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)),
      true, 0);
  db()->LoadCallback(true);
  db()->UpdateCallback(true);

  ASSERT_FALSE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       GarbageCollectLoadFailed) {
  PrepareDatabase(true);
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>()));

  // Run GC but loading the entries failed, should return an empty list.
  RunGarbageCollection(
      base::TimeDelta::FromHours(1), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)));
  db()->LoadCallback(false);
  ASSERT_TRUE(IsDataPresent(kImageKey));
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest,
       GarbageCollectUpdateFailed) {
  PrepareDatabase(true);
  EXPECT_CALL(*this, OnKeysReturned(std::vector<std::string>()));
  RunGarbageCollection(
      base::TimeDelta::FromHours(1), base::TimeDelta::FromHours(1),
      base::BindOnce(
          &CachedImageFetcherImageMetadataStoreLevelDBTest::OnKeysReturned,
          base::Unretained(this)));
  db()->LoadCallback(true);
  db()->UpdateCallback(false);
  // Update failed only simulates the callback, not the actual data behavior.
}

TEST_F(CachedImageFetcherImageMetadataStoreLevelDBTest, LoadImageMetadata) {
  PrepareDatabase(true);
  metadata_store()->SaveImageMetadata(kOtherImageKey, kImageDataLength,
                                      /* needs_transcoding */ true);

  EXPECT_CALL(*this, OnImageMetadataLoaded(_));
  metadata_store()->LoadImageMetadata(
      kOtherImageKey,
      base::BindOnce(&CachedImageFetcherImageMetadataStoreLevelDBTest::
                         OnImageMetadataLoaded,
                     base::Unretained(this)));
  db()->LoadCallback(true);
}

}  // namespace image_fetcher
