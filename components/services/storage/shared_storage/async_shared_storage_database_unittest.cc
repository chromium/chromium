// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/async_shared_storage_database.h"

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
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
using InitStatus = SharedStorageDatabase::InitStatus;
using SetBehavior = SharedStorageDatabase::SetBehavior;
using OperationResult = SharedStorageDatabase::OperationResult;
using GetResult = SharedStorageDatabase::GetResult;
using DBOperation = TestDatabaseOperationReceiver::DBOperation;
using Type = DBOperation::Type;

const int kMaxEntriesPerOrigin = 5;
const int kMaxStringLength = 100;

}  // namespace

class AsyncSharedStorageDatabaseTest : public testing::Test {
 public:
  AsyncSharedStorageDatabaseTest()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::WithBaseSyncPrimitives(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        receiver_(std::make_unique<TestDatabaseOperationReceiver>()) {}

  ~AsyncSharedStorageDatabaseTest() override = default;

  void SetUp() override {
    InitSharedStorageFeature();

    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.GetPath().AppendASCII("TestSharedStorage.db");
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();

    // It's not strictly necessary as part of test cleanup, but here we test
    // the code path that force razes the database, and, if file-backed,
    // force deletes the database file.
    EXPECT_TRUE(DestroySync());
    EXPECT_FALSE(base::PathExists(file_name_));
    EXPECT_TRUE(temp_dir_.Delete());
  }

  virtual void InitSharedStorageFeature() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageInitTries", "1"}});
  }

  // Initialize an async shared storage database instance from the SQL file at
  // `relative_file_path` in the "storage/" subdirectory of test data.
  void LoadFromFileSync(const char* relative_file_path) {
    DCHECK(!file_name_.empty());

    ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, relative_file_path));

    async_database_ = AsyncSharedStorageDatabase::Create(
        file_name_, task_runner_, special_storage_policy_,
        SharedStorageOptions::Create()->GetDatabaseOptions());
  }

  void CreateSync(const base::FilePath& db_path,
                  std::unique_ptr<SharedStorageDatabaseOptions> options) {
    async_database_ = AsyncSharedStorageDatabase::Create(
        db_path, task_runner_, special_storage_policy_, std::move(options));
  }

  void Destroy(bool* out_success) {
    DCHECK(out_success);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_DESTROY);
    auto callback =
        receiver_->MakeBoolCallback(std::move(operation), out_success);
    async_database_->Destroy(std::move(callback));
  }

  bool DestroySync() {
    if (!async_database_)
      return true;

    base::test::TestFuture<bool> future;
    async_database_->Destroy(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  bool is_finished() const {
    DCHECK(receiver_);
    return receiver_->is_finished();
  }

  void SetExpectedOperationList(std::queue<DBOperation> expected_operations) {
    DCHECK(receiver_);
    receiver_->set_expected_operations(std::move(expected_operations));
  }

  void WaitForOperations() {
    DCHECK(receiver_);
    receiver_->WaitForOperations();
  }

  void VerifySharedStorageTablesAndColumnsSync() {
    DCHECK(async_database_);

    auto task = base::BindOnce([](SharedStorageDatabase* db) -> bool {
      auto* sql_db = db->db();
      EXPECT_TRUE(sql_db);
      VerifySharedStorageTablesAndColumns(*sql_db);
      return true;
    });

    base::test::TestFuture<bool> future;

    auto wrapped_task = base::BindOnce(
        [](base::OnceCallback<bool(SharedStorageDatabase*)> task,
           base::OnceCallback<void(bool)> callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           SharedStorageDatabase* db) {
          callback_task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), std::move(task).Run(db)));
        },
        std::move(task), future.GetCallback(),
        base::SequencedTaskRunnerHandle::Get());

    async_database_->GetSequenceBoundDatabaseForTesting()
        .PostTaskWithThisObject(std::move(wrapped_task));

    EXPECT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get());
  }

  void IsOpen(bool* out_boolean) {
    DCHECK(out_boolean);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_IS_OPEN);
    auto callback =
        receiver_->MakeBoolCallback(std::move(operation), out_boolean);
    async_database_->IsOpenForTesting(std::move(callback));
  }

  bool IsOpenSync() {
    DCHECK(async_database_);

    base::test::TestFuture<bool> future;
    async_database_->IsOpenForTesting(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void DBStatus(InitStatus* out_status) {
    DCHECK(out_status);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_STATUS);
    auto callback =
        receiver_->MakeStatusCallback(std::move(operation), out_status);
    async_database_->DBStatusForTesting(std::move(callback));
  }

  InitStatus DBStatusSync() {
    DCHECK(async_database_);

    base::test::TestFuture<InitStatus> future;
    async_database_->DBStatusForTesting(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void TrimMemory() {
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_TRIM_MEMORY);
    auto callback = receiver_->MakeOnceClosure(std::move(operation));
    async_database_->TrimMemory(std::move(callback));
  }

  void OverrideLastUsedTime(url::Origin context_origin,
                            base::Time override_last_used_time,
                            bool* out_success) {
    DCHECK(out_success);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_OVERRIDE_TIME, context_origin,
                          {TestDatabaseOperationReceiver::SerializeTime(
                              override_last_used_time)});
    auto callback =
        receiver_->MakeBoolCallback(std::move(operation), out_success);
    async_database_->OverrideLastUsedTimeForTesting(std::move(context_origin),
                                                    override_last_used_time,
                                                    std::move(callback));
  }

  void Get(url::Origin context_origin,
           std::u16string key,
           GetResult* out_value) {
    DCHECK(out_value);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_GET, context_origin, {key});
    auto callback =
        receiver_->MakeGetResultCallback(std::move(operation), out_value);
    async_database_->Get(std::move(context_origin), std::move(key),
                         std::move(callback));
  }

  GetResult GetSync(url::Origin context_origin, std::u16string key) {
    DCHECK(async_database_);

    base::test::TestFuture<GetResult> future;
    async_database_->Get(std::move(context_origin), std::move(key),
                         future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void Set(url::Origin context_origin,
           std::u16string key,
           std::u16string value,
           OperationResult* out_result,
           SetBehavior behavior = SetBehavior::kDefault) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    DBOperation operation(
        Type::DB_SET, context_origin,
        {key, value, base::NumberToString16(static_cast<int>(behavior))});
    auto callback = receiver_->MakeOperationResultCallback(std::move(operation),
                                                           out_result);
    async_database_->Set(std::move(context_origin), std::move(key),
                         std::move(value), std::move(callback), behavior);
  }

  OperationResult SetSync(url::Origin context_origin,
                          std::u16string key,
                          std::u16string value,
                          SetBehavior behavior = SetBehavior::kDefault) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Set(std::move(context_origin), std::move(key),
                         std::move(value), future.GetCallback(), behavior);
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void Append(url::Origin context_origin,
              std::u16string key,
              std::u16string value,
              OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    DBOperation operation(Type::DB_APPEND, context_origin, {key, value});
    auto callback = receiver_->MakeOperationResultCallback(std::move(operation),
                                                           out_result);
    async_database_->Append(std::move(context_origin), std::move(key),
                            std::move(value), std::move(callback));
  }

  OperationResult AppendSync(url::Origin context_origin,
                             std::u16string key,
                             std::u16string value) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Append(std::move(context_origin), std::move(key),
                            std::move(value), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void Delete(url::Origin context_origin,
              std::u16string key,
              OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    DBOperation operation(Type::DB_DELETE, context_origin, {key});
    auto callback = receiver_->MakeOperationResultCallback(std::move(operation),
                                                           out_result);
    async_database_->Delete(std::move(context_origin), std::move(key),
                            std::move(callback));
  }

  OperationResult DeleteSync(url::Origin context_origin, std::u16string key) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Delete(std::move(context_origin), std::move(key),
                            future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void Length(url::Origin context_origin, int* out_length) {
    DCHECK(out_length);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_length = -1;
    DBOperation operation(Type::DB_LENGTH, context_origin);
    auto callback =
        receiver_->MakeIntCallback(std::move(operation), out_length);
    async_database_->Length(std::move(context_origin), std::move(callback));
  }

  int LengthSync(url::Origin context_origin) {
    DCHECK(async_database_);

    base::test::TestFuture<int> future;
    async_database_->Length(std::move(context_origin), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void Key(url::Origin context_origin, int index, GetResult* out_key) {
    DCHECK(out_key);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_KEY, context_origin,
                          {base::NumberToString16(index)});
    auto callback =
        receiver_->MakeGetResultCallback(std::move(operation), out_key);
    async_database_->Key(std::move(context_origin), index, std::move(callback));
  }

  GetResult KeySync(url::Origin context_origin, int index) {
    DCHECK(async_database_);

    base::test::TestFuture<GetResult> future;
    async_database_->Key(std::move(context_origin), index,
                         future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void Clear(url::Origin context_origin, OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    DBOperation operation(Type::DB_CLEAR, context_origin);
    auto callback = receiver_->MakeOperationResultCallback(std::move(operation),
                                                           out_result);
    async_database_->Clear(std::move(context_origin), std::move(callback));
  }

  OperationResult ClearSync(url::Origin context_origin) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Clear(std::move(context_origin), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void FetchOrigins(std::vector<mojom::StorageUsageInfoPtr>* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_FETCH_ORIGINS);
    auto callback =
        receiver_->MakeInfosCallback(std::move(operation), out_result);
    async_database_->FetchOrigins(std::move(callback));
  }

  std::vector<mojom::StorageUsageInfoPtr> FetchOriginsSync() {
    DCHECK(async_database_);

    base::test::TestFuture<std::vector<mojom::StorageUsageInfoPtr>> future;
    async_database_->FetchOrigins(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Take();
  }

  void PurgeMatchingOrigins(OriginMatcherFunctionUtility* matcher_utility,
                            size_t matcher_id,
                            base::Time begin,
                            base::Time end,
                            OperationResult* out_result,
                            bool perform_storage_cleanup = false) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);
    DCHECK(matcher_utility);
    DCHECK(!(*matcher_utility).is_empty());
    DCHECK_LT(matcher_id, (*matcher_utility).size());

    std::vector<std::u16string> params(
        {base::NumberToString16(matcher_id),
         TestDatabaseOperationReceiver::SerializeTime(begin),
         TestDatabaseOperationReceiver::SerializeTime(end),
         TestDatabaseOperationReceiver::SerializeBool(
             perform_storage_cleanup)});
    DBOperation operation(Type::DB_PURGE_MATCHING, std::move(params));
    auto callback = receiver_->MakeOperationResultCallback(std::move(operation),
                                                           out_result);
    async_database_->PurgeMatchingOrigins(
        matcher_utility->TakeMatcherFunctionForId(matcher_id), begin, end,
        std::move(callback), perform_storage_cleanup);
  }

  void PurgeStaleOrigins(base::TimeDelta window_to_be_deemed_active,
                         OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    DBOperation operation(Type::DB_PURGE_STALE,
                          {TestDatabaseOperationReceiver::SerializeTimeDelta(
                              window_to_be_deemed_active)});
    auto callback = receiver_->MakeOperationResultCallback(std::move(operation),
                                                           out_result);
    async_database_->PurgeStaleOrigins(window_to_be_deemed_active,
                                       std::move(callback));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<AsyncSharedStorageDatabase> async_database_;
  std::unique_ptr<TestDatabaseOperationReceiver> receiver_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
};

// Test loading version 1 database.
TEST_F(AsyncSharedStorageDatabaseTest, Version1_LoadFromFile) {
  LoadFromFileSync("shared_storage.v1.sql");
  ASSERT_TRUE(async_database_);

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(GetSync(google_com, u"key1").data, u"value1");
  EXPECT_EQ(GetSync(google_com, u"key2").data, u"value2");

  // Because the SQL database is lazy-initialized, wait to verify tables and
  // columns until after the first call to `GetSync()`.
  VerifySharedStorageTablesAndColumnsSync();

  std::vector<url::Origin> origins;
  for (const auto& info : FetchOriginsSync())
    origins.push_back(info->origin);
  EXPECT_THAT(
      origins,
      ElementsAre(url::Origin::Create(GURL("http://abc.xyz")),
                  url::Origin::Create(GURL("http://chromium.org")), google_com,
                  url::Origin::Create(GURL("http://google.org")),
                  url::Origin::Create(GURL("http://growwithgoogle.com")),
                  url::Origin::Create(GURL("http://gv.com")),
                  url::Origin::Create(GURL("http://waymo.com")),
                  url::Origin::Create(GURL("http://withgoogle.com")),
                  url::Origin::Create(GURL("http://youtube.com"))));
}

TEST_F(AsyncSharedStorageDatabaseTest, Version1_DestroyTooNew) {
  // Initialization should fail, since the last compatible version number
  // is too high.
  LoadFromFileSync("shared_storage.v1.init_too_new.sql");
  ASSERT_TRUE(async_database_);

  // Call an operation so that the database will attempt to be lazy-initialized.
  EXPECT_EQ(
      OperationResult::kInitFailure,
      SetSync(url::Origin::Create(GURL("http://www.a.com")), u"key", u"value"));
  ASSERT_FALSE(IsOpenSync());
  EXPECT_EQ(InitStatus::kTooNew, DBStatusSync());

  // Test that it is still OK to `Destroy()` the database.
  EXPECT_TRUE(DestroySync());
}

TEST_F(AsyncSharedStorageDatabaseTest, Version0_DestroyTooOld) {
  // Initialization should fail, since the current version number
  // is too low and we're forcing there not to be a retry attempt.
  LoadFromFileSync("shared_storage.v0.init_too_old.sql");
  ASSERT_TRUE(async_database_);

  // Call an operation so that the database will attempt to be lazy-initialized.
  EXPECT_EQ(
      OperationResult::kInitFailure,
      SetSync(url::Origin::Create(GURL("http://www.a.com")), u"key", u"value"));
  ASSERT_FALSE(IsOpenSync());
  EXPECT_EQ(InitStatus::kTooOld, DBStatusSync());

  // Test that it is still OK to `Destroy()` the database.
  EXPECT_TRUE(DestroySync());
}

class AsyncSharedStorageDatabaseParamTest
    : public AsyncSharedStorageDatabaseTest,
      public testing::WithParamInterface<SharedStorageWrappedBool> {
 public:
  void SetUp() override {
    AsyncSharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();

    if (GetParam().in_memory_only)
      CreateSync(base::FilePath(), std::move(options));
    else
      CreateSync(file_name_, std::move(options));
  }

  void TearDown() override {
    if (!GetParam().in_memory_only) {
      // `TearDown()` will call `DestroySync()`. First verify that the file
      // exists, so that when the we verify after destruction in `TearDown()`
      // that the file no longer exists, we know that `Destroy()` was indeed
      // successful.
      EXPECT_TRUE(base::PathExists(file_name_));
    }

    AsyncSharedStorageDatabaseTest::TearDown();
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageEntriesPerOrigin",
          base::NumberToString(kMaxEntriesPerOrigin)},
         {"MaxSharedStorageStringLength",
          base::NumberToString(kMaxStringLength)}});
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AsyncSharedStorageDatabaseParamTest,
                         testing::ValuesIn(GetSharedStorageWrappedBools()),
                         testing::PrintToStringParamName());

// Operations are tested more thoroughly in shared_storage_database_unittest.cc.
TEST_P(AsyncSharedStorageDatabaseParamTest, SyncOperations) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value2"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value2");

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key2").data, u"value1");
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin1, u"key1"));
  EXPECT_FALSE(GetSync(kOrigin1, u"key1").data);
  EXPECT_EQ(1, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data,
            base::StrCat({u"value1", u"value1"}));
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(KeySync(kOrigin1, 0).data, u"key1");
  EXPECT_EQ(KeySync(kOrigin1, 1).data, u"key2");

  EXPECT_EQ(OperationResult::kSuccess, ClearSync(kOrigin1));
  EXPECT_EQ(0, LengthSync(kOrigin1));
}

// Verifies that the async operations are executed in order and without races.
TEST_P(AsyncSharedStorageDatabaseParamTest, AsyncOperations) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));

  std::queue<DBOperation> operation_list(
      {{Type::DB_SET,
        kOrigin1,
        {u"key1", u"value1",
         TestDatabaseOperationReceiver::SerializeSetBehavior(
             SetBehavior::kDefault)}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_SET,
        kOrigin1,
        {u"key1", u"value2",
         TestDatabaseOperationReceiver::SerializeSetBehavior(
             SetBehavior::kDefault)}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_SET,
        kOrigin1,
        {u"key2", u"value1",
         TestDatabaseOperationReceiver::SerializeSetBehavior(
             SetBehavior::kDefault)}},
       {Type::DB_GET, kOrigin1, {u"key2"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_DELETE, kOrigin1, {u"key1"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_APPEND, kOrigin1, {u"key1", u"value1"}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_APPEND, kOrigin1, {u"key1", u"value1"}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_KEY, kOrigin1, {base::NumberToString16(0)}},
       {Type::DB_KEY, kOrigin1, {base::NumberToString16(1)}},
       {Type::DB_CLEAR, kOrigin1},
       {Type::DB_LENGTH, kOrigin1}});

  SetExpectedOperationList(std::move(operation_list));

  OperationResult result1 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result1);
  GetResult value1;
  Get(kOrigin1, u"key1", &value1);
  OperationResult result2 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value2", &result2);
  GetResult value2;
  Get(kOrigin1, u"key1", &value2);
  OperationResult result3 = OperationResult::kSqlError;
  Set(kOrigin1, u"key2", u"value1", &result3);
  GetResult value3;
  Get(kOrigin1, u"key2", &value3);
  int length1 = -1;
  Length(kOrigin1, &length1);

  OperationResult result4 = OperationResult::kSqlError;
  Delete(kOrigin1, u"key1", &result4);
  int length2 = -1;
  Length(kOrigin1, &length2);

  OperationResult result5 = OperationResult::kSqlError;
  Append(kOrigin1, u"key1", u"value1", &result5);
  GetResult value4;
  Get(kOrigin1, u"key1", &value4);
  int length3 = -1;
  Length(kOrigin1, &length3);

  OperationResult result6 = OperationResult::kSqlError;
  Append(kOrigin1, u"key1", u"value1", &result6);
  GetResult value5;
  Get(kOrigin1, u"key1", &value5);
  int length4 = -1;
  Length(kOrigin1, &length4);

  GetResult key1;
  Key(kOrigin1, 0, &key1);
  GetResult key2;
  Key(kOrigin1, 1, &key2);

  OperationResult result7 = OperationResult::kSqlError;
  Clear(kOrigin1, &result7);
  int length5 = -1;
  Length(kOrigin1, &length5);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  EXPECT_EQ(OperationResult::kSet, result1);
  EXPECT_EQ(value1.data, u"value1");
  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_EQ(value2.data, u"value2");
  EXPECT_EQ(OperationResult::kSet, result3);
  EXPECT_EQ(value3.data, u"value1");
  EXPECT_EQ(2, length1);

  EXPECT_EQ(OperationResult::kSuccess, result4);
  EXPECT_EQ(1, length2);

  EXPECT_EQ(OperationResult::kSet, result5);
  EXPECT_EQ(value4.data, u"value1");
  EXPECT_EQ(2, length3);

  EXPECT_EQ(OperationResult::kSet, result6);
  EXPECT_EQ(value5.data, u"value1value1");
  EXPECT_EQ(2, length4);

  EXPECT_EQ(key1.data, u"key1");
  EXPECT_EQ(key2.data, u"key2");

  EXPECT_EQ(OperationResult::kSuccess, result7);
  EXPECT_EQ(0, length5);
}

TEST_P(AsyncSharedStorageDatabaseParamTest,
       LazyInit_IgnoreForGet_CreateForSet) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));

  std::queue<DBOperation> operation_list;
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin1, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin1, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));

  SetExpectedOperationList(std::move(operation_list));

  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  // Test that we can successfully call `Get()` on a nonexistent key before the
  // database is initialized.
  GetResult value1;
  Get(kOrigin1, u"key1", &value1);
  bool open2 = false;
  IsOpen(&open2);
  InitStatus status2 = InitStatus::kUnattempted;
  DBStatus(&status2);

  // Call an operation that initializes the database.
  OperationResult result1 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result1);
  bool open3 = false;
  IsOpen(&open3);
  InitStatus status3 = InitStatus::kUnattempted;
  DBStatus(&status3);

  GetResult value2;
  Get(kOrigin1, u"key1", &value2);
  int length1 = -1;
  Length(kOrigin1, &length1);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  EXPECT_FALSE(open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  EXPECT_FALSE(value1.data);
  EXPECT_EQ(OperationResult::kSuccess, value1.result);
  EXPECT_EQ(!GetParam().in_memory_only, open2);
  EXPECT_EQ(InitStatus::kUnattempted, status2);

  EXPECT_EQ(OperationResult::kSet, result1);
  EXPECT_TRUE(open3);
  EXPECT_EQ(InitStatus::kSuccess, status3);

  EXPECT_EQ(value2.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value2.result);
  EXPECT_EQ(1, length1);
}

TEST_P(AsyncSharedStorageDatabaseParamTest,
       LazyInit_IgnoreForDelete_CreateForAppend) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));

  std::queue<DBOperation> operation_list;
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(DBOperation(Type::DB_DELETE, kOrigin1, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(
      DBOperation(Type::DB_APPEND, kOrigin1, {u"key1", u"value1"}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(DBOperation(Type::DB_DELETE, kOrigin1, {u"key2"}));

  SetExpectedOperationList(std::move(operation_list));

  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  // Test that we can successfully call `Delete()` on a nonexistent key before
  // the database is initialized.
  OperationResult result1 = OperationResult::kSqlError;
  Delete(kOrigin1, u"key1", &result1);
  bool open2 = false;
  IsOpen(&open2);
  InitStatus status2 = InitStatus::kUnattempted;
  DBStatus(&status2);

  // Call an operation that initializes the database.
  OperationResult result2 = OperationResult::kSqlError;
  Append(kOrigin1, u"key1", u"value1", &result2);
  bool open3 = false;
  IsOpen(&open3);
  InitStatus status3 = InitStatus::kUnattempted;
  DBStatus(&status3);

  // Test that we can successfully call `Delete()` on a nonexistent key after
  // the database is initialized.
  OperationResult result3 = OperationResult::kSqlError;
  Delete(kOrigin1, u"key2", &result3);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  EXPECT_FALSE(open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  EXPECT_EQ(OperationResult::kSuccess, result1);
  EXPECT_EQ(!GetParam().in_memory_only, open2);
  EXPECT_EQ(InitStatus::kUnattempted, status2);

  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_TRUE(open3);
  EXPECT_EQ(InitStatus::kSuccess, status3);

  EXPECT_EQ(OperationResult::kSuccess, result3);
}

TEST_P(AsyncSharedStorageDatabaseParamTest,
       LazyInit_IgnoreForClear_CreateForAppend) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));

  std::queue<DBOperation> operation_list;
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(DBOperation(Type::DB_CLEAR, kOrigin1));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(
      DBOperation(Type::DB_APPEND, kOrigin1, {u"key1", u"value1"}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));
  operation_list.push(DBOperation(Type::DB_CLEAR, kOrigin2));

  SetExpectedOperationList(std::move(operation_list));

  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  // Test that we can successfully call `Clear()` on a nonexistent origin before
  // the database is initialized.
  OperationResult result1 = OperationResult::kSqlError;
  Clear(kOrigin1, &result1);
  bool open2 = false;
  IsOpen(&open2);
  InitStatus status2 = InitStatus::kUnattempted;
  DBStatus(&status2);

  // Call an operation that initializes the database.
  OperationResult result2 = OperationResult::kSqlError;
  Append(kOrigin1, u"key1", u"value1", &result2);
  bool open3 = false;
  IsOpen(&open3);
  InitStatus status3 = InitStatus::kUnattempted;
  DBStatus(&status3);

  // Test that we can successfully call `Clear()` on a nonexistent origin after
  // the database is initialized.
  OperationResult result3 = OperationResult::kSqlError;
  Clear(kOrigin2, &result3);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  EXPECT_FALSE(open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  EXPECT_EQ(OperationResult::kSuccess, result1);
  EXPECT_EQ(!GetParam().in_memory_only, open2);
  EXPECT_EQ(InitStatus::kUnattempted, status2);

  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_TRUE(open3);
  EXPECT_EQ(InitStatus::kSuccess, status3);

  EXPECT_EQ(OperationResult::kSuccess, result3);
}

TEST_P(AsyncSharedStorageDatabaseParamTest, PurgeStaleOrigins) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  url::Origin kOrigin3 = url::Origin::Create(GURL("http://www.example3.test"));
  url::Origin kOrigin4 = url::Origin::Create(GURL("http://www.example4.test"));

  std::queue<DBOperation> operation_list;
  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));

  operation_list.push(DBOperation(
      Type::DB_PURGE_STALE,
      {TestDatabaseOperationReceiver::SerializeTimeDelta(base::Days(1))}));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin2,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key3", u"value3",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key3", u"value3",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key4", u"value4",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time override_time1 = base::Time::Now() - base::Days(2);
  operation_list.push(DBOperation(
      Type::DB_OVERRIDE_TIME, kOrigin1,
      {TestDatabaseOperationReceiver::SerializeTime(override_time1)}));
  operation_list.push(DBOperation(
      Type::DB_PURGE_STALE,
      {TestDatabaseOperationReceiver::SerializeTimeDelta(base::Days(1))}));

  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));
  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time override_time2 = base::Time::Now() - base::Hours(2);
  operation_list.push(DBOperation(
      Type::DB_OVERRIDE_TIME, kOrigin3,
      {TestDatabaseOperationReceiver::SerializeTime(override_time2)}));
  operation_list.push(DBOperation(
      Type::DB_PURGE_STALE,
      {TestDatabaseOperationReceiver::SerializeTimeDelta(base::Hours(1))}));

  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));

  operation_list.push(DBOperation(Type::DB_TRIM_MEMORY));
  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  operation_list.push(DBOperation(Type::DB_GET, kOrigin2, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key2"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key3"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key4"}));

  SetExpectedOperationList(std::move(operation_list));

  // Check that origin list is initially empty due to the database not being
  // initialized.
  std::vector<mojom::StorageUsageInfoPtr> infos1;
  FetchOrigins(&infos1);

  // Check that calling `PurgeStaleOrigins()` on the uninitialized database
  // doesn't give an error.
  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  OperationResult result1 = OperationResult::kSqlError;
  PurgeStaleOrigins(base::Days(1), &result1);

  OperationResult result2 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result2);

  bool open2 = false;
  IsOpen(&open2);
  InitStatus status2 = InitStatus::kUnattempted;
  DBStatus(&status2);

  OperationResult result3 = OperationResult::kSqlError;
  Set(kOrigin1, u"key2", u"value2", &result3);
  int length1 = -1;
  Length(kOrigin1, &length1);

  OperationResult result4 = OperationResult::kSqlError;
  Set(kOrigin2, u"key1", u"value1", &result4);
  int length2 = -1;
  Length(kOrigin2, &length2);

  OperationResult result5 = OperationResult::kSqlError;
  Set(kOrigin3, u"key1", u"value1", &result5);
  OperationResult result6 = OperationResult::kSqlError;
  Set(kOrigin3, u"key2", u"value2", &result6);
  OperationResult result7 = OperationResult::kSqlError;
  Set(kOrigin3, u"key3", u"value3", &result7);
  int length3 = -1;
  Length(kOrigin3, &length3);

  OperationResult result8 = OperationResult::kSqlError;
  Set(kOrigin4, u"key1", u"value1", &result8);
  OperationResult result9 = OperationResult::kSqlError;
  Set(kOrigin4, u"key2", u"value2", &result9);
  OperationResult result10 = OperationResult::kSqlError;
  Set(kOrigin4, u"key3", u"value3", &result10);
  OperationResult result11 = OperationResult::kSqlError;
  Set(kOrigin4, u"key4", u"value4", &result11);
  int length4 = -1;
  Length(kOrigin4, &length4);

  std::vector<mojom::StorageUsageInfoPtr> infos2;
  FetchOrigins(&infos2);

  bool success1 = false;
  OverrideLastUsedTime(kOrigin1, override_time1, &success1);

  OperationResult result12 = OperationResult::kSqlError;
  PurgeStaleOrigins(base::Days(1), &result12);

  int length5 = -1;
  Length(kOrigin1, &length5);
  int length6 = -1;
  Length(kOrigin2, &length6);
  int length7 = -1;
  Length(kOrigin3, &length7);
  int length8 = -1;
  Length(kOrigin4, &length8);

  std::vector<mojom::StorageUsageInfoPtr> infos3;
  FetchOrigins(&infos3);

  bool success2 = false;
  OverrideLastUsedTime(kOrigin3, override_time2, &success2);

  OperationResult result13 = OperationResult::kSqlError;
  PurgeStaleOrigins(base::Hours(1), &result13);

  int length9 = -1;
  Length(kOrigin1, &length9);
  int length10 = -1;
  Length(kOrigin2, &length10);
  int length11 = -1;
  Length(kOrigin3, &length11);
  int length12 = -1;
  Length(kOrigin4, &length12);

  TrimMemory();

  std::vector<mojom::StorageUsageInfoPtr> infos4;
  FetchOrigins(&infos4);

  GetResult value1;
  Get(kOrigin2, u"key1", &value1);
  GetResult value2;
  Get(kOrigin4, u"key1", &value2);
  GetResult value3;
  Get(kOrigin4, u"key2", &value3);
  GetResult value4;
  Get(kOrigin4, u"key3", &value4);
  GetResult value5;
  Get(kOrigin4, u"key4", &value5);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  // Database is not yet initialized. `FetchOrigins()` returns an empty vector.
  EXPECT_TRUE(infos1.empty());
  EXPECT_EQ(!GetParam().in_memory_only, open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  // No error from calling `PurgeStaleOrigins()` on an uninitialized
  // database.
  EXPECT_EQ(OperationResult::kSuccess, result1);

  // The call to `Set()` initializes the database.
  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_TRUE(open2);
  EXPECT_EQ(InitStatus::kSuccess, status2);

  EXPECT_EQ(OperationResult::kSet, result3);
  EXPECT_EQ(2, length1);

  EXPECT_EQ(OperationResult::kSet, result4);
  EXPECT_EQ(1, length2);

  EXPECT_EQ(OperationResult::kSet, result5);
  EXPECT_EQ(OperationResult::kSet, result6);
  EXPECT_EQ(OperationResult::kSet, result7);
  EXPECT_EQ(3, length3);

  EXPECT_EQ(OperationResult::kSet, result8);
  EXPECT_EQ(OperationResult::kSet, result9);
  EXPECT_EQ(OperationResult::kSet, result10);
  EXPECT_EQ(OperationResult::kSet, result11);
  EXPECT_EQ(4, length4);

  std::vector<url::Origin> origins;
  for (const auto& info : infos2)
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_TRUE(success1);
  EXPECT_EQ(OperationResult::kSuccess, result12);

  // `kOrigin1` is cleared. The other origins are not.
  EXPECT_EQ(0, length5);
  EXPECT_EQ(1, length6);
  EXPECT_EQ(3, length7);
  EXPECT_EQ(4, length8);

  origins.clear();
  for (const auto& info : infos3)
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin3, kOrigin4));

  EXPECT_TRUE(success2);
  EXPECT_EQ(OperationResult::kSuccess, result13);

  // `kOrigin3` is cleared. The other remaining ones are not.
  EXPECT_EQ(0, length9);
  EXPECT_EQ(1, length10);
  EXPECT_EQ(0, length11);
  EXPECT_EQ(4, length12);

  origins.clear();
  for (const auto& info : infos4)
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin4));

  // Database is still intact after trimming memory.
  EXPECT_EQ(value1.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value1.result);
  EXPECT_EQ(value2.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value2.result);
  EXPECT_EQ(value3.data, u"value2");
  EXPECT_EQ(OperationResult::kSuccess, value3.result);
  EXPECT_EQ(value4.data, u"value3");
  EXPECT_EQ(OperationResult::kSuccess, value4.result);
  EXPECT_EQ(value5.data, u"value4");
  EXPECT_EQ(OperationResult::kSuccess, value5.result);
}

class AsyncSharedStorageDatabasePurgeMatchingOriginsParamTest
    : public AsyncSharedStorageDatabaseTest,
      public testing::WithParamInterface<PurgeMatchingOriginsParams> {
 public:
  void SetUp() override {
    AsyncSharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();

    if (GetParam().in_memory_only)
      CreateSync(base::FilePath(), std::move(options));
    else
      CreateSync(file_name_, std::move(options));
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageEntriesPerOrigin",
          base::NumberToString(kMaxEntriesPerOrigin)},
         {"MaxSharedStorageStringLength",
          base::NumberToString(kMaxStringLength)}});
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AsyncSharedStorageDatabasePurgeMatchingOriginsParamTest,
    testing::ValuesIn(GetPurgeMatchingOriginsParams()),
    testing::PrintToStringParamName());

TEST_P(AsyncSharedStorageDatabasePurgeMatchingOriginsParamTest,
       SinceThreshold) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  url::Origin kOrigin3 = url::Origin::Create(GURL("http://www.example3.test"));
  url::Origin kOrigin4 = url::Origin::Create(GURL("http://www.example4.test"));
  url::Origin kOrigin5 = url::Origin::Create(GURL("http://www.example5.test"));

  std::queue<DBOperation> operation_list;
  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));

  base::Time threshold1 = base::Time::Now();
  OriginMatcherFunctionUtility matcher_utility;
  size_t matcher_id1 = matcher_utility.RegisterMatcherFunction({kOrigin1});

  operation_list.push(DBOperation(
      Type::DB_PURGE_MATCHING,
      {base::NumberToString16(matcher_id1),
       TestDatabaseOperationReceiver::SerializeTime(threshold1),
       TestDatabaseOperationReceiver::SerializeTime(base::Time::Max()),
       TestDatabaseOperationReceiver::SerializeBool(
           GetParam().perform_storage_cleanup)}));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_IS_OPEN));
  operation_list.push(DBOperation(Type::DB_STATUS));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin2,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key3", u"value3",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key3", u"value3",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key4", u"value4",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin5,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin5));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time threshold2 = base::Time::Now() + base::Seconds(100);
  base::Time override_time1 = threshold2 + base::Milliseconds(5);
  operation_list.push(DBOperation(
      Type::DB_OVERRIDE_TIME, kOrigin1,
      {TestDatabaseOperationReceiver::SerializeTime(override_time1)}));

  size_t matcher_id2 =
      matcher_utility.RegisterMatcherFunction({kOrigin1, kOrigin2, kOrigin5});
  operation_list.push(DBOperation(
      Type::DB_PURGE_MATCHING,
      {base::NumberToString16(matcher_id2),
       TestDatabaseOperationReceiver::SerializeTime(threshold2),
       TestDatabaseOperationReceiver::SerializeTime(base::Time::Max()),
       TestDatabaseOperationReceiver::SerializeBool(
           GetParam().perform_storage_cleanup)}));

  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin5));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time threshold3 = base::Time::Now() + base::Seconds(200);
  operation_list.push(
      DBOperation(Type::DB_OVERRIDE_TIME, kOrigin3,
                  {TestDatabaseOperationReceiver::SerializeTime(threshold3)}));

  base::Time threshold4 = threshold3 + base::Seconds(100);
  operation_list.push(
      DBOperation(Type::DB_OVERRIDE_TIME, kOrigin5,
                  {TestDatabaseOperationReceiver::SerializeTime(threshold4)}));

  size_t matcher_id3 = matcher_utility.RegisterMatcherFunction(
      {kOrigin2, kOrigin3, kOrigin4, kOrigin5});
  operation_list.push(
      DBOperation(Type::DB_PURGE_MATCHING,
                  {base::NumberToString16(matcher_id3),
                   TestDatabaseOperationReceiver::SerializeTime(threshold3),
                   TestDatabaseOperationReceiver::SerializeTime(threshold4),
                   TestDatabaseOperationReceiver::SerializeBool(
                       GetParam().perform_storage_cleanup)}));

  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin5));

  operation_list.push(DBOperation(Type::DB_TRIM_MEMORY));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  operation_list.push(DBOperation(Type::DB_GET, kOrigin2, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key2"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key3"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key4"}));

  operation_list.push(DBOperation(Type::DB_DESTROY));

  SetExpectedOperationList(std::move(operation_list));

  // Check that origin list is initially empty due to the database not being
  // initialized.
  std::vector<mojom::StorageUsageInfoPtr> infos1;
  FetchOrigins(&infos1);

  // Check that calling `PurgeMatchingOrigins()` on the uninitialized database
  // doesn't give an error.
  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  OperationResult result1 = OperationResult::kSqlError;
  PurgeMatchingOrigins(&matcher_utility, matcher_id1, threshold1,
                       base::Time::Max(), &result1,
                       GetParam().perform_storage_cleanup);

  OperationResult result2 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result2);

  bool open2 = false;
  IsOpen(&open2);
  InitStatus status2 = InitStatus::kUnattempted;
  DBStatus(&status2);

  OperationResult result3 = OperationResult::kSqlError;
  Set(kOrigin1, u"key2", u"value2", &result3);
  int length1 = -1;
  Length(kOrigin1, &length1);

  OperationResult result4 = OperationResult::kSqlError;
  Set(kOrigin2, u"key1", u"value1", &result4);
  int length2 = -1;
  Length(kOrigin2, &length2);

  OperationResult result5 = OperationResult::kSqlError;
  Set(kOrigin3, u"key1", u"value1", &result5);
  OperationResult result6 = OperationResult::kSqlError;
  Set(kOrigin3, u"key2", u"value2", &result6);
  OperationResult result7 = OperationResult::kSqlError;
  Set(kOrigin3, u"key3", u"value3", &result7);
  int length3 = -1;
  Length(kOrigin3, &length3);

  OperationResult result8 = OperationResult::kSqlError;
  Set(kOrigin4, u"key1", u"value1", &result8);
  OperationResult result9 = OperationResult::kSqlError;
  Set(kOrigin4, u"key2", u"value2", &result9);
  OperationResult result10 = OperationResult::kSqlError;
  Set(kOrigin4, u"key3", u"value3", &result10);
  OperationResult result11 = OperationResult::kSqlError;
  Set(kOrigin4, u"key4", u"value4", &result11);
  int length4 = -1;
  Length(kOrigin4, &length4);

  OperationResult result12 = OperationResult::kSqlError;
  Set(kOrigin5, u"key1", u"value1", &result12);
  int length5 = -1;
  Length(kOrigin5, &length5);

  std::vector<mojom::StorageUsageInfoPtr> infos2;
  FetchOrigins(&infos2);

  bool success1 = false;
  OverrideLastUsedTime(kOrigin1, override_time1, &success1);

  // Verify that the only match we get is for `kOrigin1`, whose `last_used_time`
  // is between the time parameters.
  OperationResult result13 = OperationResult::kSqlError;
  PurgeMatchingOrigins(&matcher_utility, matcher_id2, threshold2,
                       base::Time::Max(), &result13,
                       GetParam().perform_storage_cleanup);

  int length6 = -1;
  Length(kOrigin1, &length6);
  int length7 = -1;
  Length(kOrigin2, &length7);
  int length8 = -1;
  Length(kOrigin3, &length8);
  int length9 = -1;
  Length(kOrigin4, &length9);
  int length10 = -1;
  Length(kOrigin5, &length10);

  std::vector<mojom::StorageUsageInfoPtr> infos3;
  FetchOrigins(&infos3);

  bool success2 = false;
  OverrideLastUsedTime(kOrigin3, threshold3, &success2);
  bool success3 = false;
  OverrideLastUsedTime(kOrigin5, threshold4, &success3);

  // Verify that we still get matches for `kOrigin3`, whose `last_used_time` is
  // exactly at the `begin` time, as well as for `kOrigin5`, whose
  // `last_used_time` is exactly at the `end` time.
  OperationResult result14 = OperationResult::kSqlError;
  PurgeMatchingOrigins(&matcher_utility, matcher_id3, threshold3, threshold4,
                       &result14, GetParam().perform_storage_cleanup);

  int length11 = -1;
  Length(kOrigin1, &length11);
  int length12 = -1;
  Length(kOrigin2, &length12);
  int length13 = -1;
  Length(kOrigin3, &length13);
  int length14 = -1;
  Length(kOrigin4, &length14);
  int length15 = -1;
  Length(kOrigin5, &length15);

  TrimMemory();

  std::vector<mojom::StorageUsageInfoPtr> infos4;
  FetchOrigins(&infos4);

  GetResult value1;
  Get(kOrigin2, u"key1", &value1);
  GetResult value2;
  Get(kOrigin4, u"key1", &value2);
  GetResult value3;
  Get(kOrigin4, u"key2", &value3);
  GetResult value4;
  Get(kOrigin4, u"key3", &value4);
  GetResult value5;
  Get(kOrigin4, u"key4", &value5);

  bool success4 = false;
  Destroy(&success4);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  // Database is not yet initialized. `FetchOrigins()` returns an empty vector.
  EXPECT_TRUE(infos1.empty());
  EXPECT_EQ(!GetParam().in_memory_only, open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  // No error from calling `PurgeMatchingOrigins()` on an uninitialized
  // database.
  EXPECT_EQ(OperationResult::kSuccess, result1);

  // The call to `Set()` initializes the database.
  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_TRUE(open2);
  EXPECT_EQ(InitStatus::kSuccess, status2);

  EXPECT_EQ(OperationResult::kSet, result3);
  EXPECT_EQ(2, length1);

  EXPECT_EQ(OperationResult::kSet, result4);
  EXPECT_EQ(1, length2);

  EXPECT_EQ(OperationResult::kSet, result5);
  EXPECT_EQ(OperationResult::kSet, result6);
  EXPECT_EQ(OperationResult::kSet, result7);
  EXPECT_EQ(3, length3);

  EXPECT_EQ(OperationResult::kSet, result8);
  EXPECT_EQ(OperationResult::kSet, result9);
  EXPECT_EQ(OperationResult::kSet, result10);
  EXPECT_EQ(OperationResult::kSet, result11);
  EXPECT_EQ(4, length4);

  EXPECT_EQ(OperationResult::kSet, result12);
  EXPECT_EQ(1, length5);

  std::vector<url::Origin> origins;
  for (const auto& info : infos2)
    origins.push_back(info->origin);
  EXPECT_THAT(origins,
              ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4, kOrigin5));

  EXPECT_TRUE(success1);
  EXPECT_EQ(OperationResult::kSuccess, result13);

  // `kOrigin1` is cleared. The other origins are not.
  EXPECT_EQ(0, length6);
  EXPECT_EQ(1, length7);
  EXPECT_EQ(3, length8);
  EXPECT_EQ(4, length9);
  EXPECT_EQ(1, length10);

  origins.clear();
  for (const auto& info : infos3)
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin3, kOrigin4, kOrigin5));

  EXPECT_TRUE(success2);
  EXPECT_TRUE(success3);

  EXPECT_EQ(OperationResult::kSuccess, result14);

  // `kOrigin3` and `kOrigin5` are  cleared. The others weren't modified within
  // the given time period.
  EXPECT_EQ(0, length11);
  EXPECT_EQ(1, length12);
  EXPECT_EQ(0, length13);
  EXPECT_EQ(4, length14);
  EXPECT_EQ(0, length15);

  origins.clear();
  for (const auto& info : infos4)
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin4));

  // Database is still intact after trimming memory (and possibly performing
  // storage cleanup).
  EXPECT_EQ(value1.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value1.result);
  EXPECT_EQ(value2.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value2.result);
  EXPECT_EQ(value3.data, u"value2");
  EXPECT_EQ(OperationResult::kSuccess, value3.result);
  EXPECT_EQ(value4.data, u"value3");
  EXPECT_EQ(OperationResult::kSuccess, value4.result);
  EXPECT_EQ(value5.data, u"value4");
  EXPECT_EQ(OperationResult::kSuccess, value5.result);

  EXPECT_TRUE(success4);
}

}  // namespace storage
