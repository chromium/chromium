// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "components/services/storage/shared_storage/shared_storage_test_utils.h"
#include "sql/database.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using StorageKeyPolicyMatcherFunction =
    SharedStorageDatabase::StorageKeyPolicyMatcherFunction;
using InitStatus = SharedStorageDatabase::InitStatus;
using SetBehavior = SharedStorageDatabase::SetBehavior;
using OperationResult = SharedStorageDatabase::OperationResult;
using GetResult = SharedStorageDatabase::GetResult;
using TimeResult = SharedStorageDatabase::TimeResult;
using EntriesResult = SharedStorageDatabase::EntriesResult;

const int kBudgetIntervalHours = 24;
const int kStalenessThresholdDays = 1;
const int kBitBudget = 8;
const int kMaxBytesPerOrigin = 100;
const int kMaxBytesPerOriginForIteratorTest = 20000;
const int kMaxBatchSizeForIteratorTest = 25;

constexpr char kFileSizeKBHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.FileSize.KB";
constexpr char kNumEntriesMaxHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Max";
constexpr char kNumEntriesMinHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Min";
constexpr char kNumEntriesMedianHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Median";
constexpr char kNumEntriesQ1Histogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Q1";
constexpr char kNumEntriesQ3Histogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Q3";
constexpr char kNumEntriesTotalHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumEntries.Total";
constexpr char kNumOriginsHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.NumOrigins";
constexpr char kIsFileBackedHistogram[] =
    "Storage.SharedStorage.Database.IsFileBacked";
constexpr char kBytesUsedMaxHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.BytesUsed.PerOrigin.Max";
constexpr char kBytesUsedMinHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.BytesUsed.PerOrigin.Min";
constexpr char kBytesUsedMedianHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.BytesUsed.PerOrigin.Median";
constexpr char kBytesUsedQ1Histogram[] =
    "Storage.SharedStorage.Database.FileBacked.BytesUsed.PerOrigin.Q1";
constexpr char kBytesUsedQ3Histogram[] =
    "Storage.SharedStorage.Database.FileBacked.BytesUsed.PerOrigin.Q3";
constexpr char kBytesUsedTotalHistogram[] =
    "Storage.SharedStorage.Database.FileBacked.BytesUsed.Total.KB";
constexpr char kTimingOpenImplHistogram[] =
    "Storage.SharedStorage.Database.Timing.OpenImpl";

}  // namespace

class SharedStorageDatabaseTest : public testing::Test {
 public:
  SharedStorageDatabaseTest() {
    special_storage_policy_ = base::MakeRefCounted<MockSpecialStoragePolicy>();
  }

  ~SharedStorageDatabaseTest() override = default;

  void SetUp() override {
    InitSharedStorageFeature();

    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.GetPath().AppendASCII("TestSharedStorage.db");
  }

  void TearDown() override {
    db_.reset();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  // Initialize a shared storage database instance from the SQL file at
  // `relative_file_path` in the "storage/" subdirectory of test data.
  std::unique_ptr<SharedStorageDatabase> LoadFromFile(
      std::string relative_file_path) {
    if (!CreateDatabaseFromSQL(file_name_, relative_file_path)) {
      ADD_FAILURE() << "Failed loading " << relative_file_path;
      return nullptr;
    }

    return std::make_unique<SharedStorageDatabase>(
        file_name_, special_storage_policy_,
        SharedStorageOptions::Create()->GetDatabaseOptions());
  }

  sql::Database* SqlDB() { return db_ ? db_->db() : nullptr; }

  virtual void InitSharedStorageFeature() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageInitTries", "1"},
         {"SharedStorageBitBudget", base::NumberToString(kBitBudget)},
         {"SharedStorageBudgetInterval",
          TimeDeltaToString(base::Hours(kBudgetIntervalHours))}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<SharedStorageDatabase> db_;
  base::SimpleTestClock clock_;
  base::HistogramTester histogram_tester_;
};

// Test loading current version database.
TEST_F(SharedStorageDatabaseTest, CurrentVersion_LoadFromFile) {
  db_ = LoadFromFile(GetTestFileNameForCurrentVersion());
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  // Override the clock and set to the last time in the file that is used to
  // make a budget withdrawal.
  db_->OverrideClockForTesting(&clock_);
  clock_.SetNow(base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13269546593856733)));

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(db_->Get(google_com, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(google_com, u"key1")
                .last_used_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds(),
            13312097333991364);
  EXPECT_EQ(db_->Get(google_com, u"key2").data, u"value2");
  EXPECT_EQ(db_->Get(google_com, u"key2")
                .last_used_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds(),
            13313037427966159);

  // Because the SQL database is lazy-initialized, wait to verify tables and
  // columns until after the first call to `Get()`.
  ASSERT_TRUE(SqlDB());
  VerifySharedStorageTablesAndColumns(*SqlDB());

  url::Origin youtube_com = url::Origin::Create(GURL("http://youtube.com/"));
  EXPECT_EQ(1L, db_->Length(youtube_com));

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  EXPECT_EQ(db_->Get(chromium_org, u"a").data, u"");
  EXPECT_EQ(db_->Get(chromium_org, u"a")
                .last_used_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds(),
            13313037416916308);

  TestSharedStorageEntriesListener listener(
      task_environment_.GetMainThreadTaskRunner());
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(chromium_org, listener.BindNewPipeAndPassRemote()));
  listener.Flush();
  EXPECT_THAT(listener.TakeKeys(), ElementsAre(u"a", u"b", u"c"));
  EXPECT_EQ("", listener.error_message());
  EXPECT_EQ(1U, listener.BatchCount());
  listener.VerifyNoError();

  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  EXPECT_EQ(
      db_->Get(google_org, u"1").data,
      u"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "fffffffffffffffff");
  EXPECT_EQ(db_->Get(google_org,
                     u"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                .data,
            u"k");

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  url::Origin grow_with_google_com =
      url::Origin::Create(GURL("http://growwithgoogle.com"));
  url::Origin gv_com = url::Origin::Create(GURL("http://gv.com"));
  url::Origin waymo_com = url::Origin::Create(GURL("http://waymo.com"));
  url::Origin withgoogle_com =
      url::Origin::Create(GURL("http://withgoogle.com"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(abc_xyz, chromium_org, google_com,
                                   google_org, grow_with_google_com, gv_com,
                                   waymo_com, withgoogle_com, youtube_com));

  EXPECT_DOUBLE_EQ(kBitBudget - 5.3,
                   db_->GetRemainingBudget(net::SchemefulSite(abc_xyz)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget,
      db_->GetRemainingBudget(net::SchemefulSite(chromium_org)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget, db_->GetRemainingBudget(net::SchemefulSite(google_com)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget - 4.0,
      db_->GetRemainingBudget(net::SchemefulSite(google_org)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget - 1.2,
      db_->GetRemainingBudget(net::SchemefulSite(grow_with_google_com)).bits);
  EXPECT_DOUBLE_EQ(kBitBudget,
                   db_->GetRemainingBudget(net::SchemefulSite(gv_com)).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 4.2,
                   db_->GetRemainingBudget(net::SchemefulSite(waymo_com)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget - 1.0,
      db_->GetRemainingBudget(net::SchemefulSite(withgoogle_com)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget,
      db_->GetRemainingBudget(net::SchemefulSite(youtube_com)).bits);

  EXPECT_EQ(13266954476192362, db_->GetCreationTime(google_com)
                                   .time.ToDeltaSinceWindowsEpoch()
                                   .InMicroseconds());
  EXPECT_EQ(13266954593856733, db_->GetCreationTime(youtube_com)
                                   .time.ToDeltaSinceWindowsEpoch()
                                   .InMicroseconds());

  // Creation time for origin not present in the database will return
  // `OperationResult::kNotFound`.
  TimeResult result =
      db_->GetCreationTime(url::Origin::Create(GURL("http://a.test")));
  EXPECT_EQ(OperationResult::kNotFound, result.result);
  EXPECT_EQ(base::Time(), result.time);

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 9, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 18, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 3, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 4, 1);
  histogram_tester_.ExpectUniqueSample(
      kBytesUsedTotalHistogram,
      (16 + 16 + 28 + 30 + 32 + 40 + 46 + 46 + 4110) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 16, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 28, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram, 32, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 46, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 4110, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);

  EXPECT_TRUE(db_->Destroy());
}

// Test loading version 1 database with no budget tables.
TEST_F(SharedStorageDatabaseTest, Version1_LoadFromFileNoBudgetTables) {
  db_ = LoadFromFile("shared_storage.v1.no_budget_table.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(db_->Get(google_com, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(google_com, u"key2").data, u"value2");

  // Because the SQL database is lazy-initialized, wait to verify tables and
  // columns until after the first call to `Get()`.
  ASSERT_TRUE(SqlDB());
  VerifySharedStorageTablesAndColumns(*SqlDB());

  url::Origin youtube_com = url::Origin::Create(GURL("http://youtube.com/"));
  EXPECT_EQ(1L, db_->Length(youtube_com));

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  EXPECT_EQ(db_->Get(chromium_org, u"a").data, u"");

  TestSharedStorageEntriesListener listener(
      task_environment_.GetMainThreadTaskRunner());
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(chromium_org, listener.BindNewPipeAndPassRemote()));
  listener.Flush();
  EXPECT_THAT(listener.TakeKeys(), ElementsAre(u"a", u"b", u"c"));
  EXPECT_EQ("", listener.error_message());
  EXPECT_EQ(1U, listener.BatchCount());
  listener.VerifyNoError();

  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  EXPECT_EQ(
      db_->Get(google_org, u"1").data,
      u"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "fffffffffffffffff");
  EXPECT_EQ(db_->Get(google_org,
                     u"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                .data,
            u"k");

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  url::Origin grow_with_google_com =
      url::Origin::Create(GURL("http://growwithgoogle.com"));
  url::Origin gv_com = url::Origin::Create(GURL("http://gv.com"));
  url::Origin waymo_com = url::Origin::Create(GURL("http://waymo.com"));
  url::Origin withgoogle_com =
      url::Origin::Create(GURL("http://withgoogle.com"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(abc_xyz, chromium_org, google_com,
                                   google_org, grow_with_google_com, gv_com,
                                   waymo_com, withgoogle_com, youtube_com));

  EXPECT_DOUBLE_EQ(kBitBudget,
                   db_->GetRemainingBudget(net::SchemefulSite(abc_xyz)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget,
      db_->GetRemainingBudget(net::SchemefulSite(chromium_org)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget, db_->GetRemainingBudget(net::SchemefulSite(google_com)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget, db_->GetRemainingBudget(net::SchemefulSite(google_org)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget,
      db_->GetRemainingBudget(net::SchemefulSite(grow_with_google_com)).bits);
  EXPECT_DOUBLE_EQ(kBitBudget,
                   db_->GetRemainingBudget(net::SchemefulSite(gv_com)).bits);
  EXPECT_DOUBLE_EQ(kBitBudget,
                   db_->GetRemainingBudget(net::SchemefulSite(waymo_com)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget,
      db_->GetRemainingBudget(net::SchemefulSite(withgoogle_com)).bits);
  EXPECT_DOUBLE_EQ(
      kBitBudget,
      db_->GetRemainingBudget(net::SchemefulSite(youtube_com)).bits);

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 9, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 18, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 3, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 4, 1);
  histogram_tester_.ExpectUniqueSample(
      kBytesUsedTotalHistogram,
      (16 + 16 + 28 + 30 + 32 + 40 + 46 + 46 + 4110) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 16, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 28, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram, 32, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 46, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 4110, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);

  EXPECT_TRUE(db_->Destroy());
}

TEST_F(SharedStorageDatabaseTest, DestroyTooNew) {
  // Initialization should fail, since the last compatible version number
  // is too high.
  db_ = LoadFromFile("shared_storage.init_too_new.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());
  ASSERT_TRUE(SqlDB());

  // Call an operation so that the database will attempt to be lazy-initialized.
  const url::Origin kOrigin = url::Origin::Create(GURL("http://www.a.com"));
  EXPECT_EQ(OperationResult::kInitFailure, db_->Set(kOrigin, u"key", u"value"));
  ASSERT_FALSE(db_->IsOpenForTesting());
  EXPECT_EQ(InitStatus::kTooNew, db_->DBStatusForTesting());

  // Test that other operations likewise fail, in order to exercise these code
  // paths.
  EXPECT_EQ(OperationResult::kInitFailure, db_->Get(kOrigin, u"key").result);
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->Append(kOrigin, u"key", u"value"));
  EXPECT_EQ(OperationResult::kInitFailure, db_->Delete(kOrigin, u"key"));
  EXPECT_EQ(OperationResult::kInitFailure, db_->Clear(kOrigin));
  EXPECT_EQ(-1, db_->Length(kOrigin));
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->PurgeMatchingOrigins(StorageKeyPolicyMatcherFunction(),
                                      base::Time::Min(), base::Time::Max(),
                                      /*perform_storage_cleanup=*/false));
  EXPECT_EQ(OperationResult::kInitFailure, db_->PurgeStale());
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->GetEntriesForDevTools(kOrigin).result);

  EXPECT_EQ(OperationResult::kInitFailure,
            db_->ResetBudgetForDevTools(kOrigin));

  auto metadata = db_->GetMetadata(kOrigin);
  EXPECT_EQ(-1, metadata.length);
  EXPECT_EQ(-1, metadata.bytes_used);
  EXPECT_EQ(OperationResult::kInitFailure, metadata.time_result);
  EXPECT_EQ(OperationResult::kInitFailure, metadata.budget_result);

  // Test that it is still OK to Destroy() the database.
  EXPECT_TRUE(db_->Destroy());
}

TEST_F(SharedStorageDatabaseTest, DestroyTooOld) {
  // Initialization should fail, since the current version number
  // is too low and we're forcing there not to be a retry attempt.
  db_ = LoadFromFile(GetTestFileNameForLatestDeprecatedVersion());
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());
  ASSERT_TRUE(SqlDB());

  // Call an operation so that the database will attempt to be lazy-initialized.
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->Set(url::Origin::Create(GURL("http://www.a.com")), u"key",
                     u"value"));
  ASSERT_FALSE(db_->IsOpenForTesting());
  EXPECT_EQ(InitStatus::kTooOld, db_->DBStatusForTesting());

  // Test that it is still OK to Destroy() the database.
  EXPECT_TRUE(db_->Destroy());
}

TEST_F(SharedStorageDatabaseTest, LoadFromFile_FileOrigin) {
  db_ = LoadFromFile("shared_storage.v4.filescheme.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  // Override the clock and set to the last time in the file that is used to
  // make a budget withdrawal.
  db_->OverrideClockForTesting(&clock_);
  clock_.SetNow(base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13269546476192362)));

  url::Origin file_origin = url::Origin::Create(GURL("file://"));
  EXPECT_EQ(db_->Get(file_origin, u"a").data, u"");

  TestSharedStorageEntriesListener listener(
      task_environment_.GetMainThreadTaskRunner());
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(file_origin, listener.BindNewPipeAndPassRemote()));
  listener.Flush();
  EXPECT_THAT(listener.TakeKeys(), ElementsAre(u"a", u"b", u"c"));
  EXPECT_EQ("", listener.error_message());
  EXPECT_EQ(1U, listener.BatchCount());
  listener.VerifyNoError();

  EXPECT_EQ(OperationResult::kSet, db_->Set(file_origin, u"key1", u"value2"));
  EXPECT_EQ(db_->Get(file_origin, u"key1").data, u"value2");
  EXPECT_EQ(OperationResult::kSet,
            db_->Append(file_origin, u"key1", u"value2"));
  EXPECT_EQ(db_->Get(file_origin, u"key1").data, u"value2value2");
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(file_origin, u"key1"));
  EXPECT_EQ(OperationResult::kNotFound, db_->Get(file_origin, u"key1").result);
}

class SharedStorageDatabaseParamTest
    : public SharedStorageDatabaseTest,
      public testing::WithParamInterface<SharedStorageWrappedBool> {
 public:
  void SetUp() override {
    SharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();
    base::FilePath db_path =
        (GetParam().in_memory_only) ? base::FilePath() : file_name_;
    db_ = std::make_unique<SharedStorageDatabase>(
        db_path, special_storage_policy_, std::move(options));
    db_->OverrideClockForTesting(&clock_);
    clock_.SetNow(base::Time::Now());

    ASSERT_EQ(GetParam().in_memory_only, !db_->is_filebacked());
  }

  void TearDown() override {
    CheckInitHistograms();
    SharedStorageDatabaseTest::TearDown();
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageBytesPerOrigin",
          base::NumberToString(kMaxBytesPerOrigin)},
         {"SharedStorageBitBudget", base::NumberToString(kBitBudget)},
         {"SharedStorageBudgetInterval",
          TimeDeltaToString(base::Hours(kBudgetIntervalHours))},
         {"SharedStorageStalenessThreshold",
          TimeDeltaToString(base::Days(kStalenessThresholdDays))}});
  }

  void CheckInitHistograms() {
    histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram,
                                         db_->is_filebacked(), 1);
    if (db_->is_filebacked()) {
      histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
      EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
      histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 0, 1);
      histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 0, 1);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageDatabaseParamTest,
                         testing::ValuesIn(GetSharedStorageWrappedBools()),
                         testing::PrintToStringParamName());

TEST_P(SharedStorageDatabaseParamTest, BasicOperations) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  base::Time now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  // Verify that `last_used_time1` is set to `now` (within a tolerance).
  base::Time last_used_time1 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_LE(last_used_time1, now);
  ASSERT_GE(last_used_time1, now - TestTimeouts::action_max_timeout());

  // Advance the clock to put distance between the last used times.
  clock_.Advance(base::Hours(12));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value2"));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value2");

  // Verify that `last_used_time2` is set to `now` (within a tolerance).
  base::Time last_used_time2 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_GT(last_used_time2, last_used_time1);
  ASSERT_LE(last_used_time2, now);
  ASSERT_GE(last_used_time2, now - TestTimeouts::action_max_timeout());

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key1"));
  EXPECT_EQ(OperationResult::kNotFound, db_->Get(kOrigin1, u"key1").result);

  // Check that trying to retrieve the empty key returns
  // `OperationResult::kNotFound` rather than `OperationResult::kSqlError`,
  // even though the input is considered invalid.
  GetResult result = db_->Get(kOrigin1, u"");
  EXPECT_EQ(OperationResult::kNotFound, result.result);
  EXPECT_TRUE(result.data.empty());

  // Check that trying to delete the empty key doesn't give an error, even
  // though the input is invalid and no value is found to delete.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u""));

  // Check that trying to store and retrieve an empty value doesn't cause an
  // error.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u""));
  GetResult result2 = db_->Get(kOrigin1, u"key2");
  EXPECT_EQ(OperationResult::kSuccess, result2.result);
  EXPECT_TRUE(result2.data.empty());
}

TEST_P(SharedStorageDatabaseParamTest, IgnoreIfPresent) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  base::Time now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  // Verify that `last_used_time1` is set to `now` (within a tolerance).
  base::Time last_used_time1 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_LE(last_used_time1, now);
  ASSERT_GE(last_used_time1, now - TestTimeouts::action_max_timeout());

  // Advance the clock to put distance between the last used times.
  clock_.Advance(base::Hours(12));

  // The database does not set a new value for "key1", but retains the
  // previously set value "value1" because `behavior` is `kIgnoreIfPresent`.
  EXPECT_EQ(OperationResult::kIgnored,
            db_->Set(kOrigin1, u"key1", u"value2",
                     /*behavior=*/SetBehavior::kIgnoreIfPresent));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  // Verify that `last_used_time2` is set to `now` (within a tolerance).
  base::Time last_used_time2 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_GT(last_used_time2, last_used_time1);
  ASSERT_LE(last_used_time2, now);
  ASSERT_GE(last_used_time2, now - TestTimeouts::action_max_timeout());

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value1"));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"value1");

  // Verify that `last_used_time1` is set to `now` (within a tolerance).
  last_used_time1 = db_->Get(kOrigin1, u"key2").last_used_time;
  ASSERT_LE(last_used_time1, now);
  ASSERT_GE(last_used_time1, now - TestTimeouts::action_max_timeout());

  // Advance the clock to put distance between the last used times.
  clock_.Advance(base::Hours(12));

  // Having `behavior` set to `kDefault` makes `Set()` override any previous
  // value.
  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin1, u"key2", u"value2",
                     /*behavior=*/SetBehavior::kDefault));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"value2");

  // Verify that `last_used_time2` is set to `now` (within a tolerance).
  last_used_time2 = db_->Get(kOrigin1, u"key2").last_used_time;
  ASSERT_GT(last_used_time2, last_used_time1);
  ASSERT_LE(last_used_time2, now);
  ASSERT_GE(last_used_time2, now - TestTimeouts::action_max_timeout());

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));

  // If no previous value exists, it makes no difference whether
  // `behavior` is set to `kDefault` or `kIgnoreIfPresent`.
  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin2, u"key1", u"value1",
                     /*behavior=*/SetBehavior::kIgnoreIfPresent));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin2, u"key1").data, u"value1");

  // Verify that `last_used_time1` is set to `now` (within a tolerance).
  last_used_time1 = db_->Get(kOrigin2, u"key1").last_used_time;
  ASSERT_LE(last_used_time1, now);
  ASSERT_GE(last_used_time1, now - TestTimeouts::action_max_timeout());

  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin2, u"key2", u"value2",
                     /*behavior=*/SetBehavior::kDefault));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin2, u"key2").data, u"value2");

  // Verify that `last_used_time1` is set to `now` (within a tolerance).
  last_used_time1 = db_->Get(kOrigin2, u"key2").last_used_time;
  ASSERT_LE(last_used_time1, now);
  ASSERT_GE(last_used_time1, now - TestTimeouts::action_max_timeout());

  // Advance the clock so that the key expires.
  clock_.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  // The expired entry will be replaced instead of ignored.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"replaced",
                                            SetBehavior::kIgnoreIfPresent));
  EXPECT_EQ(db_->Get(kOrigin2, u"key2").data, u"replaced");

  // Verify that there are also no errors when setting a previously expired (but
  // unexpunged) key with the default behavior.
  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin1, u"key2", u"replaced", SetBehavior::kDefault));
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"replaced");
}

TEST_P(SharedStorageDatabaseParamTest, Append) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  base::Time now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  // Verify that `last_used_time1` is set to `now` (within a tolerance).
  base::Time last_used_time1 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_LE(last_used_time1, now);
  ASSERT_GE(last_used_time1, now - TestTimeouts::action_max_timeout());

  // Advance the clock to put distance between the last used times.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1value1");

  // Verify that `last_used_time2` is set to `now` (within a tolerance).
  base::Time last_used_time2 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_GT(last_used_time2, last_used_time1);
  ASSERT_LE(last_used_time2, now);
  ASSERT_GE(last_used_time2, now - TestTimeouts::action_max_timeout());

  // Advance the clock to put distance between the last used times.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  now = clock_.Now();
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1value1value1");

  // Verify that `last_used_time3` is set to `now` (within a tolerance).
  base::Time last_used_time3 = db_->Get(kOrigin1, u"key1").last_used_time;
  ASSERT_GT(last_used_time3, last_used_time2);
  ASSERT_LE(last_used_time3, now);
  ASSERT_GE(last_used_time3, now - TestTimeouts::action_max_timeout());

  // Advance the clock so that the key expires.
  clock_.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  // The expired entry will be replaced instead of appended to.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"replaced"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"replaced");
}

TEST_P(SharedStorageDatabaseParamTest, Get_NonUpdatedKeyExpires) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  base::Time set_time = clock_.Now();
  GetResult result1 = db_->Get(kOrigin1, u"key1");
  EXPECT_EQ(result1.data, u"value1");
  EXPECT_EQ(result1.result, OperationResult::kSuccess);

  // Verify that `result1.last_used_time` is set to `set_time` (within a
  // tolerance).
  ASSERT_LE(result1.last_used_time, set_time);
  ASSERT_GE(result1.last_used_time,
            set_time - TestTimeouts::action_max_timeout());

  // Advance the clock halfway towards expiration of the key.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));
  EXPECT_GT(clock_.Now(), set_time);

  // The information obtained is still the same.
  GetResult result2 = db_->Get(kOrigin1, u"key1");
  EXPECT_EQ(result2.data, u"value1");
  EXPECT_EQ(result2.result, OperationResult::kSuccess);
  ASSERT_LE(result2.last_used_time, set_time);
  ASSERT_GE(result2.last_used_time,
            set_time - TestTimeouts::action_max_timeout());

  // Advance the clock to key expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  // The key has expired but not been cleared yet.
  GetResult result3 = db_->Get(kOrigin1, u"key1");
  EXPECT_EQ(result3.data, u"value1");
  EXPECT_EQ(result3.result, OperationResult::kExpired);
}

TEST_P(SharedStorageDatabaseParamTest, Get_UpdatedKeyRemains) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  base::Time set_time1 = clock_.Now();
  GetResult result1 = db_->Get(kOrigin1, u"key1");
  EXPECT_EQ(result1.data, u"value1");
  EXPECT_EQ(result1.result, OperationResult::kSuccess);

  // Verify that `result1.last_used_time` is set to `set_time` (within a
  // tolerance).
  ASSERT_LE(result1.last_used_time, set_time1);
  ASSERT_GE(result1.last_used_time,
            set_time1 - TestTimeouts::action_max_timeout());

  // Advance the clock halfway towards expiration of the key.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Modify the key.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value2"));
  base::Time set_time2 = clock_.Now();
  EXPECT_GT(set_time2, set_time1);

  // The information obtained will be updated, including the `last_used_time`.
  GetResult result2 = db_->Get(kOrigin1, u"key1");
  EXPECT_EQ(result2.data, u"value2");
  EXPECT_EQ(result2.result, OperationResult::kSuccess);
  ASSERT_LE(result2.last_used_time, set_time2);
  ASSERT_GE(result2.last_used_time,
            set_time2 - TestTimeouts::action_max_timeout());

  // Advance the clock to original key expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  // The key is not cleared because it has an updated expiration time.
  GetResult result3 = db_->Get(kOrigin1, u"key1");
  EXPECT_EQ(result3.data, u"value2");
  EXPECT_EQ(result3.result, OperationResult::kSuccess);
  ASSERT_LE(result3.last_used_time, set_time2);
  ASSERT_GE(result3.last_used_time,
            set_time2 - TestTimeouts::action_max_timeout());
}

TEST_P(SharedStorageDatabaseParamTest, Length) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value3"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  // Advance the clock halfway towards expiration of the keys.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Update one entry.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value0"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));

  // Advance the clock to original key expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  // 2 keys for `kOrigin1` have now expired, so `Length()` will not count them
  // even though they have not been purged yet.
  EXPECT_EQ(1L, db_->Length(kOrigin1));
}

TEST_P(SharedStorageDatabaseParamTest, BytesUsed) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(0L, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(8 + 12, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"a", u""));
  EXPECT_EQ(8 + 12 + 2 + 0, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 0,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"a", u"b"));
  EXPECT_EQ(8 + 12 + 2 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kIgnored,
            db_->Set(kOrigin1, u"a", u"bb", SetBehavior::kIgnoreIfPresent));
  EXPECT_EQ(8 + 12 + 2 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(0L, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(0L, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin2, u"key1", u"val1"));
  EXPECT_EQ(8 + 8, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(8 + 8, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
  EXPECT_EQ(8 + 12 + 2 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin2, u"key1", u"extra"));
  EXPECT_EQ(8 + 8 + 10, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(8 + 8 + 10, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
  EXPECT_EQ(8 + 12 + 2 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin2, u"key2", u"val2"));
  EXPECT_EQ(8 + 8 + 10 + 8 + 8, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(8 + 8 + 10 + 8 + 8,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
  EXPECT_EQ(8 + 12 + 2 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(8 + 8, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(8 + 8, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
  EXPECT_EQ(8 + 12 + 2 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"v"));
  EXPECT_EQ(8 + 12 + 2 + 2 + 8 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2 + 8 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));
  EXPECT_EQ(8 + 8, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(8 + 8, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));

  // Advance the clock halfway towards expiration of the keys.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Update one entry, with no change in number of bytes.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value0"));
  EXPECT_EQ(8 + 12 + 2 + 2 + 8 + 2, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2 + 8 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  // Advance the clock to original key expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  // 2 keys for `kOrigin1` have now expired, so `BytesUsed()` will not count
  // them even though they have not been purged yet.
  EXPECT_EQ(8 + 12, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(8 + 12 + 2 + 2 + 8 + 2,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin1));

  // 1 key for `kOrigin2` has expired, so `BytesUsed()` will not count it even
  // though it has not been purged yet.
  EXPECT_EQ(0L, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(8 + 8, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));

  // Deleting an expired key will cause no error, and will purge its bytes.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key2"));
  EXPECT_EQ(0L, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(0L, db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
}

TEST_P(SharedStorageDatabaseParamTest, Keys) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_TRUE(utility.TakeKeysForId(id1).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  EXPECT_EQ(InitStatus::kUnattempted, db_->DBStatusForTesting());

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));

  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_THAT(utility.TakeKeysForId(id2), ElementsAre(u"key1", u"key2"));
  EXPECT_EQ(1U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  size_t id3 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id3)));
  utility.FlushForId(id3);
  EXPECT_TRUE(utility.TakeKeysForId(id3).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id3));
  utility.VerifyNoErrorForId(id3);

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));

  size_t id4 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id4)));
  utility.FlushForId(id4);
  EXPECT_THAT(utility.TakeKeysForId(id4),
              ElementsAre(u"key1", u"key2", u"key3"));
  EXPECT_EQ(1U, utility.BatchCountForId(id4));
  utility.VerifyNoErrorForId(id4);

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key2"));

  size_t id5 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id5)));
  utility.FlushForId(id5);
  EXPECT_THAT(utility.TakeKeysForId(id5), ElementsAre(u"key1", u"key3"));
  EXPECT_EQ(1U, utility.BatchCountForId(id5));
  utility.VerifyNoErrorForId(id5);

  // Advance the clock halfway towards expiration of the keys.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Update one entry.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value0"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  // Advance the clock to original key expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  size_t id6 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id6)));
  utility.FlushForId(id6);

  // u"key3" is now expired.
  EXPECT_THAT(utility.TakeKeysForId(id6), ElementsAre(u"key1"));
  EXPECT_EQ(1U, utility.BatchCountForId(id6));
  utility.VerifyNoErrorForId(id6);
}

TEST_P(SharedStorageDatabaseParamTest, Entries) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_TRUE(utility.TakeEntriesForId(id1).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  EXPECT_EQ(InitStatus::kUnattempted, db_->DBStatusForTesting());

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));

  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_THAT(utility.TakeEntriesForId(id2),
              ElementsAre(Pair(u"key1", u"value1"), Pair(u"key2", u"value2")));
  EXPECT_EQ(1U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  size_t id3 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id3)));
  utility.FlushForId(id3);
  EXPECT_TRUE(utility.TakeEntriesForId(id3).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id3));
  utility.VerifyNoErrorForId(id3);

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));

  size_t id4 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id4)));
  utility.FlushForId(id4);
  EXPECT_THAT(utility.TakeEntriesForId(id4),
              ElementsAre(Pair(u"key1", u"value1"), Pair(u"key2", u"value2"),
                          Pair(u"key3", u"value3")));
  EXPECT_EQ(1U, utility.BatchCountForId(id4));
  utility.VerifyNoErrorForId(id4);

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key2"));

  size_t id5 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id5)));
  utility.FlushForId(id5);
  EXPECT_THAT(utility.TakeEntriesForId(id5),
              ElementsAre(Pair(u"key1", u"value1"), Pair(u"key3", u"value3")));
  EXPECT_EQ(1U, utility.BatchCountForId(id5));
  utility.VerifyNoErrorForId(id5);

  // Advance the clock halfway towards expiration of the keys.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Update one entry.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value0"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  // Advance the clock to original key expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  size_t id6 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id6)));
  utility.FlushForId(id6);

  // u"key3" is now expired.
  EXPECT_THAT(utility.TakeEntriesForId(id6),
              ElementsAre(Pair(u"key1", u"value0")));
  EXPECT_EQ(1U, utility.BatchCountForId(id6));
  utility.VerifyNoErrorForId(id6);
}

TEST_P(SharedStorageDatabaseParamTest, Clear) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(2L, db_->Length(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
}

TEST_P(SharedStorageDatabaseParamTest, FetchOrigins) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin4));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  origins.clear();
  EXPECT_TRUE(origins.empty());
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin3, kOrigin4));
}

TEST_P(SharedStorageDatabaseParamTest, MakeBudgetWithdrawal) {
  // There should be no entries in the budget table.
  EXPECT_EQ(0L, db_->GetTotalNumBudgetEntriesForTesting());

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  const net::SchemefulSite kSite1(GURL("http://www.example1.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite1).bits);
  const net::SchemefulSite kSite2(GURL("http://www.example2.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);

  // A withdrawal for `kSite1` doesn't affect `kSite2`.
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite1, 1.75));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetTotalNumBudgetEntriesForTesting());

  // An additional withdrawal for `kSite1` at or near the same time as the
  // previous one is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite1, 2.5));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(2L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(2L, db_->GetTotalNumBudgetEntriesForTesting());

  // A withdrawal for `kSite2` doesn't affect `kSite1`.
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite2, 3.4));
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   db_->GetRemainingBudget(kSite1).bits);
  EXPECT_EQ(2L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite2));
  EXPECT_EQ(3L, db_->GetTotalNumBudgetEntriesForTesting());

  // Advance halfway through the lookback window.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2);

  // Remaining budgets continue to take into account the withdrawals above, as
  // they are still within the lookback window.
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   db_->GetRemainingBudget(kSite1).bits);

  // An additional withdrawal for `kSite1` at a later time from previous ones
  // is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite1, 1.0));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5 - 1.0,
                   db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(3L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite2));
  EXPECT_EQ(4L, db_->GetTotalNumBudgetEntriesForTesting());

  // Advance to the end of the initial lookback window, plus an additional
  // microsecond to move past that window.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2 + base::Microseconds(1));

  // Now only the single debit made within the current lookback window is
  // counted, although the entries are still in the table because we haven't
  // called `PurgeStale()`.
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(3L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite2));
  EXPECT_EQ(4L, db_->GetTotalNumBudgetEntriesForTesting());

  // After `PurgeStale()` runs, there will only be the most recent
  // debit left in the budget table.
  EXPECT_EQ(OperationResult::kSuccess, db_->PurgeStale());
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(0L, db_->GetNumBudgetEntriesForTesting(kSite2));
  EXPECT_EQ(1L, db_->GetTotalNumBudgetEntriesForTesting());

  // Advance to where the last debit should no longer be in the lookback window.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2);

  // Remaining budgets should be back at the max, although there is still an
  // entry in the table.
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetTotalNumBudgetEntriesForTesting());

  // After `PurgeStale()` runs, the budget table will be empty.
  EXPECT_EQ(OperationResult::kSuccess, db_->PurgeStale());
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(0L, db_->GetTotalNumBudgetEntriesForTesting());
}

TEST_P(SharedStorageDatabaseParamTest, ResetBudgetForDevTools) {
  // There should be no entries in the budget table.
  EXPECT_EQ(0L, db_->GetTotalNumBudgetEntriesForTesting());

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  const net::SchemefulSite kSite1(kOrigin1);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite1).bits);
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  const net::SchemefulSite kSite2(kOrigin2);
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite2).bits);

  // `kSite1` and `kSite2` are distinct.
  ASSERT_NE(kSite1, kSite2);

  // Resetting a budget in an empty uninitialized database causes no error.
  EXPECT_EQ(OperationResult::kSuccess, db_->ResetBudgetForDevTools(kOrigin1));

  // Making withdrawals will initialize the database.
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite1, 1.75));
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite1, 2.5));

  // Advance halfway through the lookback window to separate withdrawal times.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2);

  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite1, 1.0));
  EXPECT_EQ(OperationResult::kSuccess, db_->MakeBudgetWithdrawal(kSite2, 3.4));

  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5 - 1.0,
                   db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(3L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite2));
  EXPECT_EQ(4L, db_->GetTotalNumBudgetEntriesForTesting());

  // Resetting `kOrigin1`'s budget doesn't affect `kOrigin2`'s budget because
  // they correspond to distinct sites `kSite1` and `kSite2`, respetively.
  EXPECT_EQ(OperationResult::kSuccess, db_->ResetBudgetForDevTools(kOrigin1));
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, db_->GetRemainingBudget(kSite2).bits);
  EXPECT_EQ(0L, db_->GetNumBudgetEntriesForTesting(kSite1));
  EXPECT_EQ(1L, db_->GetNumBudgetEntriesForTesting(kSite2));
  EXPECT_EQ(1L, db_->GetTotalNumBudgetEntriesForTesting());

  // Resetting an already reset budget causes no error.
  EXPECT_EQ(OperationResult::kSuccess, db_->ResetBudgetForDevTools(kOrigin1));
  EXPECT_DOUBLE_EQ(kBitBudget, db_->GetRemainingBudget(kSite1).bits);
  EXPECT_EQ(0L, db_->GetNumBudgetEntriesForTesting(kSite1));

  // Resetting budget for a nonexistent origin causes no error.
  EXPECT_EQ(OperationResult::kSuccess,
            db_->ResetBudgetForDevTools(
                url::Origin::Create(GURL("http://www.example3.test"))));
}

TEST_P(SharedStorageDatabaseParamTest,
       InsertEntryBeforeExpiration_CreationTimeUnchanged) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  base::Time creation_time = db_->GetCreationTime(kOrigin1).time;

  // Advance halfway to expiration time.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Creation time does not change when `kOrigin1` inserts a new entry before
  // expiration.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(creation_time, db_->GetCreationTime(kOrigin1).time);
}

TEST_P(SharedStorageDatabaseParamTest,
       DeleteAllEntriesBeforeExpiration_CreationTimeNotFound) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(3L, db_->Length(kOrigin2));

  // Creation time will not be found when all of `kOrigin1`'s entries are
  // deleted via `Delete()` before expiration.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key1"));
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key2"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin1).result);

  // Creation time will not be found when all of `kOrigin2`'s entries are
  // deleted via `Clear()` before expiration.
  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin2).result);
}

TEST_P(SharedStorageDatabaseParamTest,
       DeleteAllEntriesAfterExpiration_CreationTimeNotFound) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  base::Time creation_time1 = db_->GetCreationTime(kOrigin1).time;

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(3L, db_->Length(kOrigin2));
  base::Time creation_time2 = db_->GetCreationTime(kOrigin2).time;

  clock_.Advance(base::Days(kStalenessThresholdDays) + base::Microseconds(1));

  // Creation time will not be found when all of `kOrigin1`'s entries are
  // deleted via `Delete()` after expiration but `PurgeStale()` has not
  // yet been called.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key1"));
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key2"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin1).result);

  // Creation time will not be found when all of `kOrigin2`'s entries are
  // deleted via `Clear()` after expiration but `PurgeStale()` has not
  // yet been called.
  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin2).result);

  EXPECT_EQ(OperationResult::kSuccess, db_->PurgeStale());

  // Creation times should still not be found after a purge of stale origins.
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin1).result);
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin2).result);

  // Creation time is updated when `kOrigin1` inserts a new entry after previous
  // expiration and purge.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_LT(creation_time1, db_->GetCreationTime(kOrigin1).time);

  // Creation time is updated when `kOrigin2` inserts a new entry after previous
  // expiration and purge.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_LT(creation_time2, db_->GetCreationTime(kOrigin2).time);
}

TEST_P(SharedStorageDatabaseParamTest,
       ClearNonexistentOrigin_NotAddedToPerOriginMapping) {
  // Initialize the database.
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));

  // Clear an origin not existing in the database. Its creation time will not be
  // found.
  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin2));
  EXPECT_EQ(OperationResult::kNotFound, db_->GetCreationTime(kOrigin2).result);
}

TEST_P(SharedStorageDatabaseParamTest, GetEntriesForDevTools) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));

  // Only `kOrigin1`'s entries are retrieved.
  EntriesResult entries_result1 = db_->GetEntriesForDevTools(kOrigin1);
  EXPECT_EQ(OperationResult::kSuccess, entries_result1.result);
  EXPECT_THAT(entries_result1.entries,
              ElementsAre(Pair("key1", "value1"), Pair("key2", "value2"),
                          Pair("key3", "value3")));

  // Advance the clock halfway towards expiration of the keys.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Update one key.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key2", u"append"));

  // Advance the clock to the expiration time for when the keys were initially
  // set.
  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0) + base::Seconds(1));

  // Only `kOrigin1`'s unexpired entries are retrieved.
  EntriesResult entries_result2 = db_->GetEntriesForDevTools(kOrigin1);
  EXPECT_EQ(OperationResult::kSuccess, entries_result2.result);
  EXPECT_THAT(entries_result2.entries,
              ElementsAre(Pair("key2", "value2append")));
}

class SharedStorageDatabasePurgeMatchingOriginsParamTest
    : public SharedStorageDatabaseTest,
      public testing::WithParamInterface<PurgeMatchingOriginsParams> {
 public:
  void SetUp() override {
    SharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();
    base::FilePath db_path =
        (GetParam().in_memory_only) ? base::FilePath() : file_name_;
    db_ = std::make_unique<SharedStorageDatabase>(
        db_path, special_storage_policy_, std::move(options));
    db_->OverrideClockForTesting(&clock_);
    clock_.SetNow(base::Time::Now());
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageBytesPerOrigin",
          base::NumberToString(kMaxBytesPerOrigin)}});
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageDatabasePurgeMatchingOriginsParamTest,
                         testing::ValuesIn(GetPurgeMatchingOriginsParams()),
                         testing::PrintToStringParamName());

TEST_P(SharedStorageDatabasePurgeMatchingOriginsParamTest, AllTime) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3));

  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin1}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));

  // `kOrigin1` is cleared. The other origins are not.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin3));

  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin2, kOrigin3}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));

  // All three origins should be cleared.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin3));

  EXPECT_TRUE(db_->FetchOrigins().empty());

  // There is no error from trying to clear an origin that isn't in the
  // database.
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
              {"http://www.example4.test"}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));
}

TEST_P(SharedStorageDatabasePurgeMatchingOriginsParamTest, SinceThreshold) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  // Origin0 is created at time 0, and key0 is written at time 0, key1 at 1
  // Origin1 is created at time 1, and key1 is written at time 1, key2 at 2
  // Origin2 is created at time 2, and key2 is written at time 2, key3 at 3
  // Origin00 is created at time 0, and key0 is written at time 0, key3 is
  // written at time 3
  const url::Origin kOrigin0 =
      url::Origin::Create(GURL("http://www.example1.test"));
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example2.test"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example3.test"));
  const url::Origin kOrigin00 =
      url::Origin::Create(GURL("http://www.example4.test"));

  // Time = 0.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin0, u"key0", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin00, u"key0", u"value1"));

  clock_.Advance(base::Milliseconds(1));

  // Time = 1.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin0, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  clock_.Advance(base::Milliseconds(1));

  // Time = 2.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value1"));
  clock_.Advance(base::Milliseconds(1));

  // Time = 3.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin00, u"key3", u"value1"));

  // Read a key from origin0 at this time. That should not cause it to get
  // purged when we purge at time 3 since only sets should cause the
  // `last_used_time` to update.
  EXPECT_EQ(db_->Get(kOrigin0, u"key0").data, u"value1");

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins()) {
    origins.push_back(info->storage_key.origin());
  }
  EXPECT_THAT(origins, ElementsAre(kOrigin0, kOrigin1, kOrigin2, kOrigin00));

  // Nothing should be deleted if the start time is in the future.
  EXPECT_EQ(OperationResult::kSuccess,
            db_->PurgeMatchingOrigins(
                StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
                    {kOrigin0, kOrigin00, kOrigin1, kOrigin2}),
                clock_.Now() + base::Milliseconds(1), base::Time::Max(),
                GetParam().perform_storage_cleanup));
  EXPECT_EQ(2L, db_->Length(kOrigin0));
  EXPECT_EQ(2L, db_->Length(kOrigin00));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(2L, db_->Length(kOrigin2));

  // Origin00 and Origin2 should be deleted if we start at the current time
  // since they both created a key then. Origin0 read a key then, but reads
  // don't update the `last_used_time`.
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin0, kOrigin00, kOrigin1, kOrigin2}),
          clock_.Now(), base::Time::Max(), GetParam().perform_storage_cleanup));
  EXPECT_EQ(2L, db_->Length(kOrigin0));
  EXPECT_EQ(0L, db_->Length(kOrigin00));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  origins.clear();
  for (const auto& info : db_->FetchOrigins()) {
    origins.push_back(info->storage_key.origin());
  }
  EXPECT_THAT(origins, ElementsAre(kOrigin0, kOrigin1));

  // Nothing should be deleted if the origins don't match.
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
              {"http://www.example5.test"}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));
  EXPECT_EQ(2L, db_->Length(kOrigin0));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  // Delete from before any keys were written, and everything should be gone.
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin0, kOrigin00, kOrigin1, kOrigin2}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));
  origins.clear();
  for (const auto& info : db_->FetchOrigins()) {
    origins.push_back(info->storage_key.origin());
  }
  EXPECT_THAT(origins, ElementsAre());
}

TEST_P(SharedStorageDatabaseParamTest, PurgeStale) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"value2");

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(db_->Get(kOrigin2, u"key1").data, u"value1");

  clock_.Advance(base::Days(kStalenessThresholdDays));
  clock_.Advance(base::Microseconds(1));

  // `Length()` does not count the expired keys.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  // Update a key and set additional keys.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin2, u"key1", u"value0", SetBehavior::kDefault));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin4));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_LT(db_->GetCreationTime(kOrigin1).time,
            db_->GetCreationTime(kOrigin3).time);
  EXPECT_LT(db_->GetCreationTime(kOrigin2).time,
            db_->GetCreationTime(kOrigin4).time);

  EXPECT_EQ(OperationResult::kSuccess, db_->PurgeStale());

  // `kOrigin1` had 1 key expire.
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  // `kOrigin2` had no keys expire.
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  // `kOrigin3` had no keys expire.
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  // `kOrigin4` had no keys expire.
  EXPECT_EQ(4L, db_->Length(kOrigin4));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));

  // Will not set the new value but will update the write time.
  EXPECT_EQ(OperationResult::kIgnored, db_->Set(kOrigin4, u"key1", u"value0",
                                                SetBehavior::kIgnoreIfPresent));

  clock_.Advance(base::Days(kStalenessThresholdDays / 2.0));
  clock_.Advance(base::Microseconds(1));

  EXPECT_EQ(OperationResult::kSuccess, db_->PurgeStale());

  // `kOrigin1` had all keys expire.
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  // `kOrigin2` had all keys expire
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  // `kOrigin3` had all keys expire
  EXPECT_EQ(0L, db_->Length(kOrigin3));

  // `kOrigin4` had all but one key expire.
  EXPECT_EQ(1L, db_->Length(kOrigin4));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin4));
}

TEST_P(SharedStorageDatabaseParamTest, TrimMemory) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin4));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin3, kOrigin4));

  // Release nonessential memory.
  db_->TrimMemory();

  // Check that the database is still intact.
  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin3, kOrigin4));

  EXPECT_EQ(1L, db_->Length(kOrigin3));
  EXPECT_EQ(2L, db_->Length(kOrigin4));

  EXPECT_EQ(db_->Get(kOrigin3, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(kOrigin4, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(kOrigin4, u"key2").data, u"value2");
}

TEST_P(SharedStorageDatabaseParamTest, Set_MaxBytesPerOrigin) {
  // Note that key-value pairs of the form (u"key" + i, u"value" + i), where i
  // is a single digit cast as a std::ustring16, all have 8 + 12 bytes total.
  // This test relies on the assumption that we meet capacity after setting
  // exactly 5 keys of the form specified.
  const int kNumBytesPerFormattedPair = 8 + 12;
  ASSERT_EQ(5 * kNumBytesPerFormattedPair, kMaxBytesPerOrigin);

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(3 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key5", u"value5"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // `kOrigin1` should have hit capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Set(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // Attempt to overwrite the value for the 1st pair with a longer value.
  // `kOrigin1` should have hit capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Set(kOrigin1, u"key1", u"value11"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // Overwrite the value for the 3rd pair with a shorter value. This will make
  // room for a small-enough pair.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u""));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair - 12, db_->BytesUsed(kOrigin1));

  // The previously tried 6th pair will still not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Set(kOrigin1, u"key6", u"value6"));

  // This smaller pair will be set.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"k6", u"v6"));
  EXPECT_EQ(6L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair - 12 + 4 + 4,
            db_->BytesUsed(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key5"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair - 12 + 4 + 4,
            db_->BytesUsed(kOrigin1));

  // There should now be enough capacity that the previously tried 6th pair will
  // be set.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(6L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair - 12 + 4 + 4,
            db_->BytesUsed(kOrigin1));
}

TEST_P(SharedStorageDatabaseParamTest, Append_MaxBytesPerOrigin) {
  // Note that key-value pairs of the form (u"key" + i, u"value" + i), where i
  // is a single digit cast as a std::ustring16, all have 8 + 12 bytes total.
  // This test relies on the assumption that we meet capacity after setting
  // exactly 5 keys of the form specified.
  const int kNumBytesPerFormattedPair = 8 + 12;
  ASSERT_EQ(5 * kNumBytesPerFormattedPair, kMaxBytesPerOrigin);

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(3 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key5", u"value5"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // `kOrigin1` should have hit capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Append(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // Attempt to append to the value for the 1st pair. `kOrigin1` should have hit
  // capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity, db_->Append(kOrigin1, u"key1", u"1"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key5"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // There should now be enough capacity, and trying to append a character to
  // the first pair should succeed.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"1"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair + 2, db_->BytesUsed(kOrigin1));

  // There still will not be capacity for the 6th pair as previously tried.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Append(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair + 2, db_->BytesUsed(kOrigin1));

  // Setting a slightly shorter 6th pair will succeed.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key6", u"value"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair + 2 + kNumBytesPerFormattedPair - 2,
            db_->BytesUsed(kOrigin1));
}

TEST_P(SharedStorageDatabaseParamTest,
       Set_MaxBytesPerOrigin_AdvanceTime_PurgeTriggered) {
  // Note that key-value pairs of the form (u"key" + i, u"value" + i), where i
  // is a single digit cast as a std::ustring16, all have 8 + 12 bytes total.
  // This test relies on the assumption that we meet capacity after setting
  // exactly 5 keys of the form specified.
  const int kNumBytesPerFormattedPair = 8 + 12;
  ASSERT_EQ(5 * kNumBytesPerFormattedPair, kMaxBytesPerOrigin);

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(3 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key5", u"value5"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // `kOrigin1` should have hit capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Set(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin2));

  // Advance the clock so that the above keys expire.
  clock_.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  // Reattempting to set the 6th pair should succeed, and moreover clear out the
  // expired keys for `kOrigin1`.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // `kOrigin2` still has its expired key.
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));

  // `kOrigin2` has enough quota to set this pair without triggering a purge.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
}

TEST_P(SharedStorageDatabaseParamTest,
       Append_MaxBytesPerOrigin_AdvanceTime_PurgeTriggered) {
  // Note that key-value pairs of the form (u"key" + i, u"value" + i), where i
  // is a single digit cast as a std::ustring16, all have 8 + 12 bytes total.
  // This test relies on the assumption that we meet capacity after setting
  // exactly 5 keys of the form specified.
  const int kNumBytesPerFormattedPair = 8 + 12;
  ASSERT_EQ(5 * kNumBytesPerFormattedPair, kMaxBytesPerOrigin);

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(3 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(4 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key5", u"value5"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // `kOrigin1` should have hit capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Append(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(5 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin3));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin4));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin4));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin4));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin4));

  const url::Origin kOrigin5 =
      url::Origin::Create(GURL("http://www.example5.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin5, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin5));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin5));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin5, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin5));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin5));

  // Advance the clock so that the above keys expire.
  clock_.Advance(base::Days(kStalenessThresholdDays) + base::Seconds(1));

  // Reattempting to set the 6th pair via `Append()` should succeed, and
  // moreover clear out the expired keys for `kOrigin1`.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin1));

  // `kOrigin2`, `kOrigin3`, `kOrigin4, and `kOrigin5` still have their expired
  // keys.
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin3));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin3));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin3));
  EXPECT_EQ(0L, db_->Length(kOrigin4));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin4));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin4));
  EXPECT_EQ(0L, db_->Length(kOrigin5));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin5));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin5));

  // `kOrigin3` has enough quota to set this pair via `Append()` without
  // triggering a purge.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair, db_->BytesUsed(kOrigin2));
  EXPECT_EQ(2 * kNumBytesPerFormattedPair,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin2));

  // Note that the pair (u"key*", kLongValue) would alone exactly take up an
  // entire origin's quota.
  const std::u16string kLongValue(kMaxBytesPerOrigin / 2 - 4, 'v');
  ASSERT_EQ(static_cast<size_t>(kMaxBytesPerOrigin),
            2 * (std::u16string(u"key*").size() + kLongValue.size()));

  // There will be just enough room to set the following pair via `Append()`,
  // although it will trigger a purge of the expired key.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin3, u"key2", kLongValue));
  EXPECT_EQ(1L, db_->Length(kOrigin3));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin3));
  EXPECT_EQ(kMaxBytesPerOrigin,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin3));

  // Trying to append `kLongValue` to the expired key-value pair for `kOrigin4`
  // will actually set a fresh key-value pair after triggering a purge of the
  // origin's expired keys due to needing to make room.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin4, u"key1", kLongValue));
  EXPECT_EQ(1L, db_->Length(kOrigin4));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin4));
  EXPECT_EQ(kMaxBytesPerOrigin,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin4));

  // Trying to append a short value to the expired key-value pair for `kOrigin5`
  // will actually set a fresh key-value pair without triggering a purge of the
  // origin's other expired key.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin5, u"key1", u"short"));
  EXPECT_EQ(1L, db_->Length(kOrigin5));
  EXPECT_EQ(8 + 10, db_->BytesUsed(kOrigin5));
  EXPECT_EQ(1 * kNumBytesPerFormattedPair + 8 + 10,
            db_->NumBytesUsedIncludeExpiredForTesting(kOrigin5));
}

TEST_P(SharedStorageDatabaseParamTest, InvalidAppend) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"k", u"v"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(2 + 2, db_->BytesUsed(kOrigin1));

  const size_t kMaxStringLength = static_cast<size_t>(kMaxBytesPerOrigin) / 2;
  const std::u16string kLongValue(kMaxStringLength, u'v');

  EXPECT_EQ(OperationResult::kInvalidAppend,
            db_->Append(kOrigin1, u"k", kLongValue));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(2 + 2, db_->BytesUsed(kOrigin1));
}

TEST_P(SharedStorageDatabaseParamTest, MaxKeyLength) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  const size_t kMaxStringLength = static_cast<size_t>(kMaxBytesPerOrigin) / 2;
  const std::u16string kLongKey(kMaxStringLength, u'k');

  // This key has the maximum allowed length. Setting will succeed since the
  // value is empty and the origin is currently empty.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, kLongKey, u""));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Appending to the value would exceed the allowed capacity and so will fail.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Append(kOrigin1, kLongKey, u"a"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Deleting a key of the maximum allowed length causes no error.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, kLongKey));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  // Attempting to retrieve a non-existent maximum length key causes no error.
  EXPECT_EQ(db_->Get(kOrigin1, kLongKey).result, OperationResult::kNotFound);

  // Append should be able to set a key of maximum length with an empty value
  // when the origin is empty.
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, kLongKey, u""));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Retrieving the maximum length key succeeds.
  auto get_result = db_->Get(kOrigin1, kLongKey);
  EXPECT_EQ(OperationResult::kSuccess, get_result.result);
  EXPECT_EQ(u"", get_result.data);
}

TEST_P(SharedStorageDatabaseParamTest, MaxValueLength) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  const size_t kMaxStringLength = static_cast<size_t>(kMaxBytesPerOrigin) / 2;
  const std::u16string kLongValue(kMaxStringLength, u'v');
  const std::u16string kAlmostLongValue(kMaxStringLength - 1, u'v');

  // This value has the maximum allowed length. Setting will fail due to
  // exceeding capacity, but the database will not crash.
  EXPECT_EQ(OperationResult::kNoCapacity, db_->Set(kOrigin1, u"k", kLongValue));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  // Try again with a value that is one char16 shorter. This time the value
  // should be set.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"k", kAlmostLongValue));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Appending to the value would exceed the allowed length and so will fail.
  EXPECT_EQ(OperationResult::kInvalidAppend,
            db_->Append(kOrigin1, u"k", u"aa"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Deleting an entry whose value has the effective maximum allowed length
  // (i.e. `kMaxStringLength - 1`) causes no error.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"k"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  // Append likewise will not crash with a value of maximum length, but will
  // fail due to insufficient capacity.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Append(kOrigin1, u"k", kLongValue));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  // Append should be able to set a value of effective maximum length (i.e.
  // `kMaxStringLength - 1`) with a key of length 1 when the origin is empty.
  EXPECT_EQ(OperationResult::kSet,
            db_->Append(kOrigin1, u"k", kAlmostLongValue));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Retrieving a value of effective maximum length (i.e. `kMaxStringLength -
  // 1`) succeeds.
  auto get_result = db_->Get(kOrigin1, u"k");
  EXPECT_EQ(OperationResult::kSuccess, get_result.result);
  EXPECT_EQ(kAlmostLongValue, get_result.data);
}

TEST_P(SharedStorageDatabaseParamTest, AppendToMaxValueLength) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  const size_t kMaxStringLength = static_cast<size_t>(kMaxBytesPerOrigin) / 2;
  const std::u16string kHalfLongValue1(kMaxStringLength / 2, u'v');
  const std::u16string kHalfLongValue2(kMaxStringLength / 2 - 1, u'v');

  EXPECT_EQ(OperationResult::kSet,
            db_->Append(kOrigin1, u"k", kHalfLongValue1));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(2 * static_cast<int64_t>(1 + kMaxStringLength / 2),
            db_->BytesUsed(kOrigin1));

  EXPECT_EQ(OperationResult::kSet,
            db_->Append(kOrigin1, u"k", kHalfLongValue2));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Appending to the value again will fail.
  EXPECT_EQ(OperationResult::kNoCapacity, db_->Append(kOrigin1, u"k", u"a"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));

  // Trying to append to the value again with a longer value will also fail,
  // with `OperationResult::kInvalidAppend`.
  EXPECT_EQ(OperationResult::kInvalidAppend,
            db_->Append(kOrigin1, u"k", u"aa"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(kMaxBytesPerOrigin, db_->BytesUsed(kOrigin1));
}

TEST_P(SharedStorageDatabaseParamTest, MaxKeyLengthAndMaxValueLength) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  const size_t kMaxStringLength = static_cast<size_t>(kMaxBytesPerOrigin) / 2;
  const std::u16string kLongKey(kMaxStringLength, u'k');
  const std::u16string kLongValue(kMaxStringLength, u'v');

  // Failure due to no capacity when both key and value are maximum length will
  // be handled gracefully (no crash).
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Set(kOrigin1, kLongKey, kLongValue));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));

  // Failure due to no capacity when both key and value are maximum length will
  // be handled gracefully (no crash).
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Append(kOrigin1, kLongKey, kLongValue));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0, db_->BytesUsed(kOrigin1));
}

class SharedStorageDatabaseIteratorTest : public SharedStorageDatabaseTest {
 public:
  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageBytesPerOrigin",
          base::NumberToString(kMaxBytesPerOriginForIteratorTest)},
         {"MaxSharedStorageIteratorBatchSize",
          base::NumberToString(kMaxBatchSizeForIteratorTest)}});
  }
};

TEST_F(SharedStorageDatabaseIteratorTest, Keys) {
  db_ = LoadFromFile("shared_storage.v3.iterator.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  // Override the clock and set to the last time in the file that is used to set
  // a `last_used_time` for a value.
  db_->OverrideClockForTesting(&clock_);
  clock_.SetNow(base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13268941676192362)));

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(google_com, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_EQ(201U, utility.TakeKeysForId(id1).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(9U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->Keys(chromium_org, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_EQ(26U, utility.TakeKeysForId(id2).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(2U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 227, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 26, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 26, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 113.5, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 201, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 201, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedTotalHistogram,
                                       (364 + 5196) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 364, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 364, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram,
                                       (364 + 5196) / 2, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 5196, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 5196, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

TEST_F(SharedStorageDatabaseIteratorTest, Entries) {
  db_ = LoadFromFile("shared_storage.v3.iterator.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  // Override the clock and set to the last time in the file that is used to set
  // a `last_used_time` for a value.
  db_->OverrideClockForTesting(&clock_);
  clock_.SetNow(base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13268941676192362)));

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->Entries(google_com, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_EQ(201U, utility.TakeEntriesForId(id1).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(9U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->Entries(chromium_org, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_EQ(26U, utility.TakeEntriesForId(id2).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(2U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 227, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 26, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 26, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 113.5, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 201, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 201, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedTotalHistogram,
                                       (364 + 5196) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 364, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 364, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram,
                                       (364 + 5196) / 2, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 5196, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 5196, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

// Tests correct calculation of five-number summary when there is only one
// origin.
TEST_F(SharedStorageDatabaseTest, SingleOrigin) {
  db_ = LoadFromFile("shared_storage.v6.single_origin.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(google_com));

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedTotalHistogram, 200 / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 200, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 200, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram, 200, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 200, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 200, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

// Tests correct calculation of five-number summary when number of origins is
// greater than one and has remainder 1 modulo 4.
TEST_F(SharedStorageDatabaseTest, FiveOrigins) {
  db_ = LoadFromFile("shared_storage.v6.empty_values_mapping.5origins.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  url::Origin gv_com = url::Origin::Create(GURL("http://gv.com"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(abc_xyz, chromium_org, google_com,
                                   google_org, gv_com));

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 5, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 335, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 12.5, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 20, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 145, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 250, 1);
  histogram_tester_.ExpectUniqueSample(
      kBytesUsedTotalHistogram, (2500 + 4000 + 2000 + 10000 + 150) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 150, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, (150 + 2000) / 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram, 2500, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram,
                                       (10000 + 4000) / 2, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 10000, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

// Tests correct calculation of five-number summary when number of origins has
// remainder 2 modulo 4.
TEST_F(SharedStorageDatabaseTest, SixOrigins) {
  db_ = LoadFromFile("shared_storage.v6.empty_values_mapping.6origins.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  url::Origin gv_com = url::Origin::Create(GURL("http://gv.com"));
  url::Origin waymo_com = url::Origin::Create(GURL("http://waymo.com"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(abc_xyz, chromium_org, google_com,
                                   google_org, gv_com, waymo_com));

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 6, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 1934, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 15, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 30, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 250, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 1599, 1);
  histogram_tester_.ExpectUniqueSample(
      kBytesUsedTotalHistogram,
      (2500 + 4000 + 2000 + 10000 + 150 + 1599000) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 150, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 2000, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram,
                                       (2500 + 4000) / 2, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 10000, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 1599000, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

// Tests correct calculation of five-number summary when number of origins has
// remainder 3 modulo 4.
TEST_F(SharedStorageDatabaseTest, SevenOrigins) {
  db_ = LoadFromFile("shared_storage.v6.empty_values_mapping.7origins.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  url::Origin gv_com = url::Origin::Create(GURL("http://gv.com"));
  url::Origin waymo_com = url::Origin::Create(GURL("http://waymo.com"));
  url::Origin with_google_com =
      url::Origin::Create(GURL("http://withgoogle.com"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins,
              ElementsAre(abc_xyz, chromium_org, google_com, google_org, gv_com,
                          waymo_com, with_google_com));

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 7, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 2935, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 15, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 40, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 1001, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 1599, 1);
  histogram_tester_.ExpectUniqueSample(
      kBytesUsedTotalHistogram,
      (2500 + 4000 + 2000 + 10000 + 150 + 1599000 + 100100) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 150, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, 2000, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram, 4000, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram, 100100, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 1599000, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

// Tests correct calculation of five-number summary when number of origins has
// remainder 0 modulo 4.
TEST_F(SharedStorageDatabaseTest, EightOrigins) {
  db_ = LoadFromFile("shared_storage.v6.empty_values_mapping.8origins.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(db_->is_filebacked());

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  url::Origin gv_com = url::Origin::Create(GURL("http://gv.com"));
  url::Origin waymo_com = url::Origin::Create(GURL("http://waymo.com"));
  url::Origin with_google_com =
      url::Origin::Create(GURL("http://withgoogle.com"));
  url::Origin youtube_com = url::Origin::Create(GURL("http://youtube.com/"));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins,
              ElementsAre(abc_xyz, chromium_org, google_com, google_org, gv_com,
                          waymo_com, with_google_com, youtube_com));

  histogram_tester_.ExpectUniqueSample(kIsFileBackedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kFileSizeKBHistogram, 1);
  EXPECT_GT(histogram_tester_.GetTotalSum(kFileSizeKBHistogram), 0);
  histogram_tester_.ExpectUniqueSample(kNumOriginsHistogram, 8, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesTotalHistogram, 3035, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMinHistogram, 10, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ1Histogram, 17.5, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMedianHistogram, 70, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesQ3Histogram, 625.5, 1);
  histogram_tester_.ExpectUniqueSample(kNumEntriesMaxHistogram, 1599, 1);
  histogram_tester_.ExpectUniqueSample(
      kBytesUsedTotalHistogram,
      (2500 + 4000 + 2000 + 10000 + 150 + 1599000 + 100100 + 1000) / 1024, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMinHistogram, 150, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ1Histogram, (1000 + 2000) / 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMedianHistogram,
                                       (2500 + 4000) / 2, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedQ3Histogram,
                                       (10000 + 100100) / 2, 1);
  histogram_tester_.ExpectUniqueSample(kBytesUsedMaxHistogram, 1599000, 1);
  histogram_tester_.ExpectTotalCount(kTimingOpenImplHistogram, 1);
}

}  // namespace storage
