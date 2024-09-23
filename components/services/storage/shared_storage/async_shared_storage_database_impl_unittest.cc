// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/async_shared_storage_database_impl.h"

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "components/services/storage/shared_storage/shared_storage_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "sql/database.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
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
using BudgetResult = SharedStorageDatabase::BudgetResult;
using DBOperation = TestDatabaseOperationReceiver::DBOperation;
using Type = DBOperation::Type;
using DBType = SharedStorageTestDBType;

const int kBudgetIntervalHours = 24;
const int kStalenessThresholdDays = 1;
const int kBitBudget = 8;
const int kMaxBytesPerOrigin = 100;

}  // namespace

class AsyncSharedStorageDatabaseImplTest : public testing::Test {
 public:
  AsyncSharedStorageDatabaseImplTest()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::WithBaseSyncPrimitives(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        receiver_(std::make_unique<TestDatabaseOperationReceiver>()) {}

  ~AsyncSharedStorageDatabaseImplTest() override = default;

  void SetUp() override {
    InitSharedStorageFeature();
    async_database_ = Create();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();

    // It's not strictly necessary as part of test cleanup, but here we test
    // the code path that force razes the database, and, if file-backed,
    // force deletes the database file.
    EXPECT_TRUE(DestroySync());
    EXPECT_FALSE(base::PathExists(file_name_));
    if (GetType() != DBType::kInMemory)
      EXPECT_TRUE(temp_dir_.Delete());
  }

  virtual void InitSharedStorageFeature() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageInitTries", "1"},
         {"SharedStorageBitBudget", base::NumberToString(kBitBudget)},
         {"SharedStorageBudgetInterval",
          TimeDeltaToString(base::Hours(kBudgetIntervalHours))}});
  }

  virtual DBType GetType() { return DBType::kInMemory; }

  // Return the relative file path in the "storage/" subdirectory of test data
  // for the SQL file from which to initialize an async shared storage database
  // instance.
  virtual std::string GetRelativeFilePath() { return nullptr; }

  std::unique_ptr<AsyncSharedStorageDatabase> Create() {
    if (GetType() != DBType::kInMemory)
      PrepareFileBacked();
    else
      EXPECT_TRUE(file_name_.empty());

    return AsyncSharedStorageDatabaseImpl::Create(
        file_name_, task_runner_, special_storage_policy_,
        SharedStorageOptions::Create()->GetDatabaseOptions());
  }

  void PrepareFileBacked() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_name_ = temp_dir_.GetPath().AppendASCII("TestSharedStorage.db");
    if (GetType() == DBType::kFileBackedFromExisting)
      ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, GetRelativeFilePath()));
    EXPECT_FALSE(file_name_.empty());
  }

  AsyncSharedStorageDatabaseImpl* GetImpl() {
    return static_cast<AsyncSharedStorageDatabaseImpl*>(async_database_.get());
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

  void RunTask(base::OnceCallback<bool(SharedStorageDatabase*)> task) {
    DCHECK(async_database_);
    DCHECK(GetImpl()->GetSequenceBoundDatabaseForTesting());

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
        base::SequencedTaskRunner::GetCurrentDefault());

    GetImpl()->GetSequenceBoundDatabaseForTesting()->PostTaskWithThisObject(
        std::move(wrapped_task));

    EXPECT_TRUE(future.Get());
  }

  void VerifySharedStorageTablesAndColumnsSync() {
    auto task = base::BindOnce([](SharedStorageDatabase* db) -> bool {
      auto* sql_db = db->db();
      EXPECT_TRUE(sql_db);
      VerifySharedStorageTablesAndColumns(*sql_db);
      return true;
    });

    RunTask(std::move(task));
  }

  void IsOpen(bool* out_boolean) {
    DCHECK(out_boolean);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback =
        receiver_->MakeBoolCallback(DBOperation(Type::DB_IS_OPEN), out_boolean);
    GetImpl()->IsOpenForTesting(std::move(callback));
  }

  bool IsOpenSync() {
    DCHECK(async_database_);

    base::test::TestFuture<bool> future;
    GetImpl()->IsOpenForTesting(future.GetCallback());
    return future.Get();
  }

  void DBStatus(InitStatus* out_status) {
    DCHECK(out_status);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback =
        receiver_->MakeStatusCallback(DBOperation(Type::DB_STATUS), out_status);
    GetImpl()->DBStatusForTesting(std::move(callback));
  }

  InitStatus DBStatusSync() {
    DCHECK(async_database_);

    base::test::TestFuture<InitStatus> future;
    GetImpl()->DBStatusForTesting(future.GetCallback());
    return future.Get();
  }

  void OverrideCreationTime(url::Origin context_origin,
                            base::Time new_creation_time,
                            bool* out_success) {
    DCHECK(out_success);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeBoolCallback(
        DBOperation(
            Type::DB_OVERRIDE_TIME_ORIGIN, context_origin,
            {TestDatabaseOperationReceiver::SerializeTime(new_creation_time)}),
        out_success);
    GetImpl()->OverrideCreationTimeForTesting(
        std::move(context_origin), new_creation_time, std::move(callback));
  }

  void OverrideLastUsedTime(url::Origin context_origin,
                            std::u16string key,
                            base::Time new_last_used_time,
                            bool* out_success) {
    DCHECK(out_success);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeBoolCallback(
        DBOperation(Type::DB_OVERRIDE_TIME_ENTRY, context_origin,
                    {key, TestDatabaseOperationReceiver::SerializeTime(
                              new_last_used_time)}),
        out_success);
    GetImpl()->OverrideLastUsedTimeForTesting(std::move(context_origin), key,
                                              new_last_used_time,
                                              std::move(callback));
  }

  void OverrideClockSync(base::Clock* clock) {
    DCHECK(async_database_);

    base::RunLoop loop;
    GetImpl()->OverrideClockForTesting(clock, loop.QuitClosure());
    loop.Run();
  }

  void AdvanceClockAsync(base::TimeDelta delta) {
    auto task = base::BindOnce(
        [](base::SimpleTestClock* clock, base::TimeDelta delta,
           SharedStorageDatabase* db) -> bool {
          clock->Advance(delta);
          return true;
        },
        &clock_, delta);

    RunTask(std::move(task));
  }

  void GetNumBudgetEntries(net::SchemefulSite context_site, int* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = -1;
    auto callback = receiver_->MakeIntCallback(
        DBOperation(Type::DB_GET_NUM_BUDGET, context_site), out_result);
    GetImpl()->GetNumBudgetEntriesForTesting(std::move(context_site),
                                             std::move(callback));
  }

  int GetNumBudgetEntriesSync(net::SchemefulSite context_site) {
    DCHECK(async_database_);

    base::test::TestFuture<int> future;
    GetImpl()->GetNumBudgetEntriesForTesting(std::move(context_site),
                                             future.GetCallback());
    return future.Get();
  }

  void GetTotalNumBudgetEntries(int* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = -1;
    auto callback = receiver_->MakeIntCallback(
        DBOperation(Type::DB_GET_TOTAL_NUM_BUDGET), out_result);
    GetImpl()->GetTotalNumBudgetEntriesForTesting(std::move(callback));
  }

  int GetTotalNumBudgetEntriesSync() {
    DCHECK(async_database_);

    base::test::TestFuture<int> future;
    GetImpl()->GetTotalNumBudgetEntriesForTesting(future.GetCallback());
    return future.Get();
  }

  void TrimMemory() {
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback =
        receiver_->MakeOnceClosure(DBOperation(Type::DB_TRIM_MEMORY));
    async_database_->TrimMemory(std::move(callback));
  }

  void Get(url::Origin context_origin,
           std::u16string key,
           GetResult* out_value) {
    DCHECK(out_value);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeGetResultCallback(
        DBOperation(Type::DB_GET, context_origin, {key}), out_value);
    async_database_->Get(std::move(context_origin), std::move(key),
                         std::move(callback));
  }

  GetResult GetSync(url::Origin context_origin, std::u16string key) {
    DCHECK(async_database_);

    base::test::TestFuture<GetResult> future;
    async_database_->Get(std::move(context_origin), std::move(key),
                         future.GetCallback());
    return future.Take();
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
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(
            Type::DB_SET, context_origin,
            {key, value,
             TestDatabaseOperationReceiver::SerializeSetBehavior(behavior)}),
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
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_APPEND, context_origin, {key, value}), out_result);
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
    return future.Get();
  }

  void Delete(url::Origin context_origin,
              std::u16string key,
              OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_DELETE, context_origin, {key}), out_result);
    async_database_->Delete(std::move(context_origin), std::move(key),
                            std::move(callback));
  }

  OperationResult DeleteSync(url::Origin context_origin, std::u16string key) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Delete(std::move(context_origin), std::move(key),
                            future.GetCallback());
    return future.Get();
  }

  void Length(url::Origin context_origin, int* out_length) {
    DCHECK(out_length);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_length = -1;
    auto callback = receiver_->MakeIntCallback(
        DBOperation(Type::DB_LENGTH, context_origin), out_length);
    async_database_->Length(std::move(context_origin), std::move(callback));
  }

  int LengthSync(url::Origin context_origin) {
    DCHECK(async_database_);

    base::test::TestFuture<int> future;
    async_database_->Length(std::move(context_origin), future.GetCallback());
    return future.Get();
  }

  void Keys(url::Origin context_origin,
            TestSharedStorageEntriesListenerUtility* listener_utility,
            int listener_id,
            OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_KEYS, context_origin,
                    {base::NumberToString16(listener_id)}),
        out_result);
    async_database_->Keys(
        std::move(context_origin),
        listener_utility->BindNewPipeAndPassRemoteForId(listener_id),
        std::move(callback));
  }

  OperationResult KeysSync(
      url::Origin context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          listener) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Keys(std::move(context_origin), std::move(listener),
                          future.GetCallback());
    return future.Get();
  }

  void Entries(url::Origin context_origin,
               TestSharedStorageEntriesListenerUtility* listener_utility,
               int listener_id,
               OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_ENTRIES, context_origin,
                    {base::NumberToString16(listener_id)}),
        out_result);
    async_database_->Entries(
        std::move(context_origin),
        listener_utility->BindNewPipeAndPassRemoteForId(listener_id),
        std::move(callback));
  }

  OperationResult EntriesSync(
      url::Origin context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          listener) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Entries(std::move(context_origin), std::move(listener),
                             future.GetCallback());
    return future.Get();
  }

  void Clear(url::Origin context_origin, OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_CLEAR, context_origin), out_result);
    async_database_->Clear(std::move(context_origin), std::move(callback));
  }

  OperationResult ClearSync(url::Origin context_origin) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->Clear(std::move(context_origin), future.GetCallback());
    return future.Get();
  }

  void FetchOrigins(std::vector<mojom::StorageUsageInfoPtr>* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeInfosCallback(
        DBOperation(Type::DB_FETCH_ORIGINS), out_result);
    async_database_->FetchOrigins(std::move(callback));
  }

  std::vector<mojom::StorageUsageInfoPtr> FetchOriginsSync() {
    DCHECK(async_database_);

    base::test::TestFuture<std::vector<mojom::StorageUsageInfoPtr>> future;
    async_database_->FetchOrigins(future.GetCallback());
    return future.Take();
  }

  void PurgeMatchingOrigins(
      StorageKeyPolicyMatcherFunctionUtility* matcher_utility,
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
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_PURGE_MATCHING, std::move(params)), out_result);
    async_database_->PurgeMatchingOrigins(
        matcher_utility->TakeMatcherFunctionForId(matcher_id), begin, end,
        std::move(callback), perform_storage_cleanup);
  }

  void PurgeStale(OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_PURGE_STALE), out_result);
    async_database_->PurgeStale(std::move(callback));
  }

  OperationResult PurgeStaleSync() {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->PurgeStale(future.GetCallback());
    return future.Get();
  }

  void MakeBudgetWithdrawal(net::SchemefulSite context_site,
                            double bits_debit,
                            OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_MAKE_BUDGET_WITHDRAWAL, context_site,
                    {base::NumberToString16(bits_debit)}),
        out_result);
    async_database_->MakeBudgetWithdrawal(std::move(context_site), bits_debit,
                                          std::move(callback));
  }

  OperationResult MakeBudgetWithdrawalSync(
      const net::SchemefulSite& context_site,
      double bits_debit) {
    DCHECK(async_database_);

    base::test::TestFuture<OperationResult> future;
    async_database_->MakeBudgetWithdrawal(std::move(context_site), bits_debit,
                                          future.GetCallback());
    return future.Get();
  }

  void GetRemainingBudget(net::SchemefulSite context_site,
                          BudgetResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeBudgetResultCallback(
        DBOperation(Type::DB_GET_REMAINING_BUDGET, context_site), out_result);
    async_database_->GetRemainingBudget(std::move(context_site),
                                        std::move(callback));
  }

  BudgetResult GetRemainingBudgetSync(const net::SchemefulSite& context_site) {
    DCHECK(async_database_);

    base::test::TestFuture<BudgetResult> future;
    async_database_->GetRemainingBudget(std::move(context_site),
                                        future.GetCallback());
    return future.Take();
  }

  void GetCreationTime(url::Origin context_origin, TimeResult* out_result) {
    DCHECK(out_result);
    DCHECK(async_database_);
    DCHECK(receiver_);

    auto callback = receiver_->MakeTimeResultCallback(
        DBOperation(Type::DB_GET_CREATION_TIME, context_origin), out_result);
    async_database_->GetCreationTime(std::move(context_origin),
                                     std::move(callback));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<AsyncSharedStorageDatabase> async_database_;
  std::unique_ptr<TestDatabaseOperationReceiver> receiver_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
  base::SimpleTestClock clock_;
};

class AsyncSharedStorageDatabaseImplFromFileTest
    : public AsyncSharedStorageDatabaseImplTest {
 public:
  DBType GetType() override { return DBType::kFileBackedFromExisting; }

  std::string GetRelativeFilePath() override {
    return GetTestFileNameForCurrentVersion();
  }
};

// Test loading current version database.
TEST_F(AsyncSharedStorageDatabaseImplFromFileTest,
       CurrentVersion_LoadFromFile) {
  ASSERT_TRUE(async_database_);

  // Override the clock and set to the last time in the file that is used to
  // make a budget withdrawal.
  OverrideClockSync(&clock_);
  clock_.SetNow(base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13269546593856733)));

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(GetSync(google_com, u"key1").data, u"value1");
  EXPECT_EQ(GetSync(google_com, u"key2").data, u"value2");

  // Because the SQL database is lazy-initialized, wait to verify tables and
  // columns until after the first call to `GetSync()`.
  VerifySharedStorageTablesAndColumnsSync();

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));

  std::vector<url::Origin> origins;
  for (const auto& info : FetchOriginsSync())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(
      origins,
      ElementsAre(abc_xyz, url::Origin::Create(GURL("http://chromium.org")),
                  google_com, url::Origin::Create(GURL("http://google.org")),
                  url::Origin::Create(GURL("http://growwithgoogle.com")),
                  url::Origin::Create(GURL("http://gv.com")),
                  url::Origin::Create(GURL("http://waymo.com")),
                  url::Origin::Create(GURL("http://withgoogle.com")),
                  url::Origin::Create(GURL("http://youtube.com"))));

  EXPECT_DOUBLE_EQ(kBitBudget - 5.3,
                   GetRemainingBudgetSync(net::SchemefulSite(abc_xyz)).bits);
  EXPECT_DOUBLE_EQ(kBitBudget,
                   GetRemainingBudgetSync(net::SchemefulSite(google_com)).bits);
}

class AsyncSharedStorageDatabaseImplFromFileV1NoBudgetTableTest
    : public AsyncSharedStorageDatabaseImplFromFileTest {
 public:
  std::string GetRelativeFilePath() override {
    return "shared_storage.v1.no_budget_table.sql";
  }
};

// Test loading version 1 database without budget table.
TEST_F(AsyncSharedStorageDatabaseImplFromFileV1NoBudgetTableTest,
       Version1_LoadFromFileNoBudgetTable) {
  ASSERT_TRUE(async_database_);

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(GetSync(google_com, u"key1").data, u"value1");
  EXPECT_EQ(GetSync(google_com, u"key2").data, u"value2");

  // Because the SQL database is lazy-initialized, wait to verify tables and
  // columns until after the first call to `GetSync()`.
  VerifySharedStorageTablesAndColumnsSync();

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));

  std::vector<url::Origin> origins;
  for (const auto& info : FetchOriginsSync())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(
      origins,
      ElementsAre(abc_xyz, url::Origin::Create(GURL("http://chromium.org")),
                  google_com, url::Origin::Create(GURL("http://google.org")),
                  url::Origin::Create(GURL("http://growwithgoogle.com")),
                  url::Origin::Create(GURL("http://gv.com")),
                  url::Origin::Create(GURL("http://waymo.com")),
                  url::Origin::Create(GURL("http://withgoogle.com")),
                  url::Origin::Create(GURL("http://youtube.com"))));

  EXPECT_DOUBLE_EQ(kBitBudget,
                   GetRemainingBudgetSync(net::SchemefulSite(abc_xyz)).bits);
  EXPECT_DOUBLE_EQ(kBitBudget,
                   GetRemainingBudgetSync(net::SchemefulSite(google_com)).bits);
}

struct InitFailureTestCase {
  std::string relative_file_path;
  InitStatus expected_status;
};

std::vector<InitFailureTestCase> GetInitFailureTestCases() {
  return std::vector<InitFailureTestCase>(
      {{"shared_storage.init_too_new.sql", InitStatus::kTooNew},
       {GetTestFileNameForLatestDeprecatedVersion(), InitStatus::kTooOld}});
}

// Used by `testing::PrintToStringParamName()`.
[[nodiscard]] std::string PrintToString(const InitFailureTestCase& c) {
  std::string str(c.relative_file_path);
  for (char& ch : str) {
    if (!absl::ascii_isalpha(static_cast<unsigned char>(ch)) && ch != '_') {
      ch = '_';
    }
  }
  return str;
}

class AsyncSharedStorageDatabaseImplFromFileWithFailureTest
    : public AsyncSharedStorageDatabaseImplTest,
      public testing::WithParamInterface<InitFailureTestCase> {
 public:
  DBType GetType() override { return DBType::kFileBackedFromExisting; }

  std::string GetRelativeFilePath() override {
    return GetParam().relative_file_path;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AsyncSharedStorageDatabaseImplFromFileWithFailureTest,
                         testing::ValuesIn(GetInitFailureTestCases()),
                         testing::PrintToStringParamName());

TEST_P(AsyncSharedStorageDatabaseImplFromFileWithFailureTest, Destroy) {
  ASSERT_TRUE(async_database_);

  // Call an operation so that the database will attempt to be lazy-initialized.
  EXPECT_EQ(
      OperationResult::kInitFailure,
      SetSync(url::Origin::Create(GURL("http://www.a.com")), u"key", u"value"));
  ASSERT_FALSE(IsOpenSync());
  EXPECT_EQ(GetParam().expected_status, DBStatusSync());

  // Test that it is still OK to `Destroy()` the database.
  EXPECT_TRUE(DestroySync());
}

class AsyncSharedStorageDatabaseImplParamTest
    : public AsyncSharedStorageDatabaseImplTest,
      public testing::WithParamInterface<SharedStorageWrappedBool> {
 public:
  void TearDown() override {
    if (!GetParam().in_memory_only) {
      // `TearDown()` will call `DestroySync()`. First verify that the file
      // exists, so that when the we verify after destruction in `TearDown()`
      // that the file no longer exists, we know that `Destroy()` was indeed
      // successful.
      EXPECT_TRUE(base::PathExists(file_name_));
    }

    AsyncSharedStorageDatabaseImplTest::TearDown();
  }

  DBType GetType() override {
    return GetParam().in_memory_only ? DBType::kInMemory
                                     : DBType::kFileBackedFromNew;
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
};

INSTANTIATE_TEST_SUITE_P(All,
                         AsyncSharedStorageDatabaseImplParamTest,
                         testing::ValuesIn(GetSharedStorageWrappedBools()),
                         testing::PrintToStringParamName());

// Operations are tested more thoroughly in shared_storage_database_unittest.cc.
TEST_P(AsyncSharedStorageDatabaseImplParamTest, SyncOperations) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value2"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value2");

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key2").data, u"value1");
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin1, u"key1"));
  EXPECT_EQ(OperationResult::kNotFound, GetSync(kOrigin1, u"key1").result);
  EXPECT_EQ(1, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data,
            base::StrCat({u"value1", u"value1"}));
  EXPECT_EQ(2, LengthSync(kOrigin1));

  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      KeysSync(kOrigin1, listener_utility.BindNewPipeAndPassRemoteForId(id1)));
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeKeysForId(id1),
              ElementsAre(u"key1", u"key2"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);

  size_t id2 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(kOrigin1,
                        listener_utility.BindNewPipeAndPassRemoteForId(id2)));
  listener_utility.FlushForId(id2);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id2),
              ElementsAre(std::make_pair(u"key1", u"value1value1"),
                          std::make_pair(u"key2", u"value1")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  EXPECT_EQ(OperationResult::kSuccess, ClearSync(kOrigin1));
  EXPECT_EQ(0, LengthSync(kOrigin1));
}

// Synchronously tests budget operations.
TEST_P(AsyncSharedStorageDatabaseImplParamTest, SyncMakeBudgetWithdrawal) {
  OverrideClockSync(&clock_);
  clock_.SetNow(base::Time::Now());

  // There should be no entries in the budget table.
  EXPECT_EQ(0, GetTotalNumBudgetEntriesSync());

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  const net::SchemefulSite kSite1(GURL("http://www.example1.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite1).bits);
  const net::SchemefulSite kSite2(GURL("http://www.example2.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);

  // A withdrawal for `kSite1` doesn't affect `kSite2`.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kSite1, 1.75));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75, GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(1, GetTotalNumBudgetEntriesSync());

  // An additional withdrawal for `kSite1` at or near the same time as the
  // previous one is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kSite1, 2.5));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(2, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(2, GetTotalNumBudgetEntriesSync());

  // A withdrawal for `kSite2` doesn't affect `kSite1`.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kSite2, 3.4));
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   GetRemainingBudgetSync(kSite1).bits);
  EXPECT_EQ(2, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kSite2));
  EXPECT_EQ(3, GetTotalNumBudgetEntriesSync());

  // Advance halfway through the lookback window.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2);

  // Remaining budgets continue to take into account the withdrawals above, as
  // they are still within the lookback window.
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   GetRemainingBudgetSync(kSite1).bits);

  // An additional withdrawal for `kSite1` at a later time from previous ones
  // is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kSite1, 1.0));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5 - 1.0,
                   GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(3, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kSite2));
  EXPECT_EQ(4, GetTotalNumBudgetEntriesSync());

  // Advance to the end of the initial lookback window, plus an additional
  // microsecond to move past that window.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2 + base::Microseconds(1));

  // Now only the single debit made within the current lookback window is
  // counted, although the entries are still in the table because we haven't
  // called `PurgeStaleSync()`.
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(3, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kSite2));
  EXPECT_EQ(4, GetTotalNumBudgetEntriesSync());

  // After `PurgeStaleSync()` runs, there will only be the most
  // recent debit left in the budget table
  EXPECT_EQ(OperationResult::kSuccess, PurgeStaleSync());
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(0, GetNumBudgetEntriesSync(kSite2));
  EXPECT_EQ(1, GetTotalNumBudgetEntriesSync());

  // Advance to where the last debit should no longer be in the lookback window.
  clock_.Advance(base::Hours(kBudgetIntervalHours) / 2);

  // Remaining budgets should be back at the max, although there is still an
  // entry in the table.
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kSite1));
  EXPECT_EQ(1, GetTotalNumBudgetEntriesSync());

  // After `PurgeStaleSync()` runs, the budget table will be empty.
  EXPECT_EQ(OperationResult::kSuccess, PurgeStaleSync());
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kSite2).bits);
  EXPECT_EQ(0, GetTotalNumBudgetEntriesSync());
}

// Verifies that the async operations are executed in order and without races.
TEST_P(AsyncSharedStorageDatabaseImplParamTest, AsyncOperations) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  size_t id2 = listener_utility.RegisterListener();

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
       {Type::DB_KEYS, kOrigin1, {base::NumberToString16(id1)}},
       {Type::DB_ENTRIES, kOrigin1, {base::NumberToString16(id2)}},
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

  OperationResult result7 = OperationResult::kSqlError;
  Keys(kOrigin1, &listener_utility, id1, &result7);

  OperationResult result8 = OperationResult::kSqlError;
  Entries(kOrigin1, &listener_utility, id2, &result8);

  OperationResult result9 = OperationResult::kSqlError;
  Clear(kOrigin1, &result9);
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

  EXPECT_EQ(OperationResult::kSuccess, result7);
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeKeysForId(id1),
              ElementsAre(u"key1", u"key2"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);
  listener_utility.FlushForId(id2);

  EXPECT_EQ(OperationResult::kSuccess, result8);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id2),
              ElementsAre(std::make_pair(u"key1", u"value1value1"),
                          std::make_pair(u"key2", u"value1")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  EXPECT_EQ(OperationResult::kSuccess, result9);
  EXPECT_EQ(0, length5);
}

TEST_P(AsyncSharedStorageDatabaseImplParamTest, AsyncMakeBudgetWithdrawal) {
  OverrideClockSync(&clock_);
  clock_.SetNow(base::Time::Now());

  const net::SchemefulSite kSite1(GURL("http://www.example1.test"));
  const net::SchemefulSite kSite2(GURL("http://www.example2.test"));

  // need to fix operations
  std::queue<DBOperation> operation_list;
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);

  operation_list.emplace(
      Type::DB_MAKE_BUDGET_WITHDRAWAL, kSite1,
      std::vector<std::u16string>({base::NumberToString16(1.75)}));
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(
      Type::DB_MAKE_BUDGET_WITHDRAWAL, kSite1,
      std::vector<std::u16string>({base::NumberToString16(2.5)}));
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(
      Type::DB_MAKE_BUDGET_WITHDRAWAL, kSite2,
      std::vector<std::u16string>({base::NumberToString16(3.4)}));
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);

  operation_list.emplace(
      Type::DB_MAKE_BUDGET_WITHDRAWAL, kSite1,
      std::vector<std::u16string>({base::NumberToString16(1.0)}));
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(Type::DB_PURGE_STALE);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_NUM_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  operation_list.emplace(Type::DB_PURGE_STALE);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite1);
  operation_list.emplace(Type::DB_GET_REMAINING_BUDGET, kSite2);
  operation_list.emplace(Type::DB_GET_TOTAL_NUM_BUDGET);

  SetExpectedOperationList(std::move(operation_list));

  // There should be no entries in the budget table.
  int total_entries1 = -1;
  GetTotalNumBudgetEntries(&total_entries1);

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  BudgetResult budget_result1 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result1);
  BudgetResult budget_result2 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result2);

  // A withdrawal for `kSite1` doesn't affect `kSite2`.
  OperationResult operation_result1 = OperationResult::kSqlError;
  MakeBudgetWithdrawal(kSite1, 1.75, &operation_result1);
  BudgetResult budget_result3 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result3);
  BudgetResult budget_result4 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result4);
  int num_entries1 = -1;
  GetNumBudgetEntries(kSite1, &num_entries1);
  int total_entries2 = -1;
  GetTotalNumBudgetEntries(&total_entries2);

  // An additional withdrawal for `kSite1` at or near the same time as the
  // previous one is debited appropriately.
  OperationResult operation_result2 = OperationResult::kSqlError;
  MakeBudgetWithdrawal(kSite1, 2.5, &operation_result2);
  BudgetResult budget_result5 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result5);
  BudgetResult budget_result6 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result6);
  int num_entries2 = -1;
  GetNumBudgetEntries(kSite1, &num_entries2);
  int total_entries3 = -1;
  GetTotalNumBudgetEntries(&total_entries3);

  // A withdrawal for `kSite2` doesn't affect `kSite1`.
  OperationResult operation_result3 = OperationResult::kSqlError;
  MakeBudgetWithdrawal(kSite2, 3.4, &operation_result3);
  BudgetResult budget_result7 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result7);
  BudgetResult budget_result8 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result8);
  int num_entries3 = -1;
  GetNumBudgetEntries(kSite1, &num_entries3);
  int num_entries4 = -1;
  GetNumBudgetEntries(kSite2, &num_entries4);
  int total_entries4 = -1;
  GetTotalNumBudgetEntries(&total_entries4);

  // Advance halfway through the lookback window.
  AdvanceClockAsync(base::Hours(kBudgetIntervalHours / 2));

  // Remaining budgets continue to take into account the withdrawals above, as
  // they are still within the lookback window.
  BudgetResult budget_result9 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result9);
  BudgetResult budget_result10 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result10);

  // An additional withdrawal for `kSite1` at a later time from previous ones
  // is debited appropriately.
  OperationResult operation_result4 = OperationResult::kSqlError;
  MakeBudgetWithdrawal(kSite1, 1.0, &operation_result4);
  BudgetResult budget_result11 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result11);
  BudgetResult budget_result12 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result12);
  int num_entries5 = -1;
  GetNumBudgetEntries(kSite1, &num_entries5);
  int num_entries6 = -1;
  GetNumBudgetEntries(kSite2, &num_entries6);
  int total_entries5 = -1;
  GetTotalNumBudgetEntries(&total_entries5);

  // Advance to the end of the initial lookback window, plus an additional
  // microsecond to move past that window.
  AdvanceClockAsync(base::Hours(kBudgetIntervalHours / 2) +
                    base::Microseconds(1));

  // Now only the single debit made within the current lookback window is
  // counted, although the entries are still in the table because we haven't
  // called `PurgeStale()`.
  BudgetResult budget_result13 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result13);
  BudgetResult budget_result14 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result14);
  int num_entries7 = -1;
  GetNumBudgetEntries(kSite1, &num_entries7);
  int num_entries8 = -1;
  GetNumBudgetEntries(kSite2, &num_entries8);
  int total_entries6 = -1;
  GetTotalNumBudgetEntries(&total_entries6);

  // After `PurgeStale()` runs, there will only be the most
  // recent debit left in the budget table.
  OperationResult operation_result5 = OperationResult::kSqlError;
  PurgeStale(&operation_result5);
  BudgetResult budget_result15 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result15);
  BudgetResult budget_result16 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result16);
  int num_entries9 = -1;
  GetNumBudgetEntries(kSite1, &num_entries9);
  int num_entries10 = -1;
  GetNumBudgetEntries(kSite2, &num_entries10);
  int total_entries7 = -1;
  GetTotalNumBudgetEntries(&total_entries7);

  // Advance to where the last debit should no longer be in the lookback window.
  AdvanceClockAsync(base::Hours(kBudgetIntervalHours / 2));

  // Remaining budgets should be back at the max, although there is still an
  // entry in the table.
  BudgetResult budget_result17 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result17);
  BudgetResult budget_result18 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result18);
  int num_entries11 = -1;
  GetNumBudgetEntries(kSite1, &num_entries11);
  int total_entries8 = -1;
  GetTotalNumBudgetEntries(&total_entries8);

  // After `PurgeStale()` runs, the budget table will be empty.
  OperationResult operation_result6 = OperationResult::kSqlError;
  PurgeStale(&operation_result6);
  BudgetResult budget_result19 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite1, &budget_result19);
  BudgetResult budget_result20 = MakeBudgetResultForSqlError();
  GetRemainingBudget(kSite2, &budget_result20);
  int total_entries9 = -1;
  GetTotalNumBudgetEntries(&total_entries9);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  // There should be no entries in the budget table.
  EXPECT_EQ(0, total_entries1);

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result1.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result2.bits);

  // A withdrawal for `kSite1` doesn't affect `kSite2`.
  EXPECT_EQ(OperationResult::kSuccess, operation_result1);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75, budget_result3.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result4.bits);
  EXPECT_EQ(1, num_entries1);
  EXPECT_EQ(1, total_entries2);

  // An additional withdrawal for `kSite1` at or near the same time as the
  // previous one is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, operation_result2);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5, budget_result5.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result6.bits);
  EXPECT_EQ(2, num_entries2);
  EXPECT_EQ(2, total_entries3);

  // A withdrawal for `kSite2` doesn't affect `kSite1`.
  EXPECT_EQ(OperationResult::kSuccess, operation_result3);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, budget_result7.bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5, budget_result8.bits);
  EXPECT_EQ(2, num_entries3);
  EXPECT_EQ(1, num_entries4);
  EXPECT_EQ(3, total_entries4);

  // Remaining budgets continue to take into account the withdrawals above, as
  // they are still within the lookback window.
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, budget_result9.bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5, budget_result10.bits);

  // An additional withdrawal for `kSite1` at a later time from previous ones
  // is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, operation_result4);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5 - 1.0, budget_result11.bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, budget_result12.bits);
  EXPECT_EQ(3, num_entries5);
  EXPECT_EQ(1, num_entries6);
  EXPECT_EQ(4, total_entries5);

  // Now only the single debit made within the current lookback window is
  // counted, although the entries are still in the table because we haven't
  // called `PurgeStale()`.
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, budget_result13.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result14.bits);
  EXPECT_EQ(3, num_entries7);
  EXPECT_EQ(1, num_entries8);
  EXPECT_EQ(4, total_entries6);

  // After `PurgeStale()` runs, there will only be the most
  // recent debit left in the budget table.
  EXPECT_EQ(OperationResult::kSuccess, operation_result5);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, budget_result15.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result16.bits);
  EXPECT_EQ(1, num_entries9);
  EXPECT_EQ(0, num_entries10);
  EXPECT_EQ(1, total_entries7);

  // Remaining budgets should be back at the max, although there is still an
  // entry in the table.
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result17.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result18.bits);
  EXPECT_EQ(1, num_entries11);
  EXPECT_EQ(1, total_entries8);

  // After `PurgeStale()` runs, the budget table will be empty.
  EXPECT_EQ(OperationResult::kSuccess, operation_result6);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result19.bits);
  EXPECT_DOUBLE_EQ(kBitBudget, budget_result20.bits);
  EXPECT_EQ(0, total_entries9);
}

TEST_P(AsyncSharedStorageDatabaseImplParamTest,
       LazyInit_IgnoreForGet_CreateForSet) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));

  std::queue<DBOperation> operation_list;
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_GET, kOrigin1,
                         std::vector<std::u16string>({u"key1"}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(
      Type::DB_SET, kOrigin1,
      std::vector<std::u16string>(
          {u"key1", u"value1",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_GET, kOrigin1,
                         std::vector<std::u16string>({u"key1"}));
  operation_list.emplace(Type::DB_LENGTH, kOrigin1);

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

  EXPECT_TRUE(value1.data.empty());
  EXPECT_EQ(OperationResult::kNotFound, value1.result);
  EXPECT_EQ(!GetParam().in_memory_only, open2);
  EXPECT_EQ(InitStatus::kUnattempted, status2);

  EXPECT_EQ(OperationResult::kSet, result1);
  EXPECT_TRUE(open3);
  EXPECT_EQ(InitStatus::kSuccess, status3);

  EXPECT_EQ(value2.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value2.result);
  EXPECT_EQ(1, length1);
}

TEST_P(AsyncSharedStorageDatabaseImplParamTest,
       LazyInit_IgnoreForDelete_CreateForAppend) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));

  std::queue<DBOperation> operation_list;
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_DELETE, kOrigin1,
                         std::vector<std::u16string>({u"key1"}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_APPEND, kOrigin1,
                         std::vector<std::u16string>({u"key1", u"value1"}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_DELETE, kOrigin1,
                         std::vector<std::u16string>({u"key2"}));

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

TEST_P(AsyncSharedStorageDatabaseImplParamTest,
       LazyInit_IgnoreForClear_CreateForAppend) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));

  std::queue<DBOperation> operation_list;
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_CLEAR, kOrigin1);
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_APPEND, kOrigin1,
                         std::vector<std::u16string>({u"key1", u"value1"}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_CLEAR, kOrigin2);

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

TEST_P(AsyncSharedStorageDatabaseImplParamTest,
       LazyInit_IgnoreForGetCreationTime_CreateForSet) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));

  std::queue<DBOperation> operation_list;
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_GET_CREATION_TIME, kOrigin1);
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(
      Type::DB_SET, kOrigin1,
      std::vector<std::u16string>(
          {u"key1", u"value1",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);
  operation_list.emplace(Type::DB_GET_CREATION_TIME, kOrigin2);

  SetExpectedOperationList(std::move(operation_list));

  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  // Test that we can successfully call `GetCreationTime()` on a nonexistent
  // origin before the database is initialized.
  TimeResult result1;
  GetCreationTime(kOrigin1, &result1);
  bool open2 = false;
  IsOpen(&open2);
  InitStatus status2 = InitStatus::kUnattempted;
  DBStatus(&status2);

  // Call an operation that initializes the database.
  OperationResult result2 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result2);
  bool open3 = false;
  IsOpen(&open3);
  InitStatus status3 = InitStatus::kUnattempted;
  DBStatus(&status3);

  // Test that we can successfully call `GetCreationTime()` on a nonexistent
  // origin after the database is initialized.
  TimeResult result3;
  GetCreationTime(kOrigin2, &result3);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  EXPECT_FALSE(open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  EXPECT_EQ(OperationResult::kNotFound, result1.result);
  EXPECT_EQ(!GetParam().in_memory_only, open2);
  EXPECT_EQ(InitStatus::kUnattempted, status2);

  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_TRUE(open3);
  EXPECT_EQ(InitStatus::kSuccess, status3);

  EXPECT_EQ(OperationResult::kNotFound, result3.result);
}

TEST_P(AsyncSharedStorageDatabaseImplParamTest, PurgeStale) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  url::Origin kOrigin3 = url::Origin::Create(GURL("http://www.example3.test"));
  url::Origin kOrigin4 = url::Origin::Create(GURL("http://www.example4.test"));

  std::queue<DBOperation> operation_list;
  operation_list.emplace(Type::DB_FETCH_ORIGINS);
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);

  operation_list.emplace(Type::DB_PURGE_STALE);

  operation_list.emplace(
      Type::DB_SET, kOrigin1,
      std::vector<std::u16string>(
          {u"key1", u"value1",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_IS_OPEN);
  operation_list.emplace(Type::DB_STATUS);

  operation_list.emplace(
      Type::DB_SET, kOrigin1,
      std::vector<std::u16string>(
          {u"key2", u"value2",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_LENGTH, kOrigin1);

  operation_list.emplace(
      Type::DB_SET, kOrigin2,
      std::vector<std::u16string>(
          {u"key1", u"value1",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_LENGTH, kOrigin2);

  operation_list.emplace(
      Type::DB_SET, kOrigin3,
      std::vector<std::u16string>(
          {u"key1", u"value1",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(
      Type::DB_SET, kOrigin3,
      std::vector<std::u16string>(
          {u"key2", u"value2",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(
      Type::DB_SET, kOrigin3,
      std::vector<std::u16string>(
          {u"key3", u"value3",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_LENGTH, kOrigin3);

  operation_list.emplace(
      Type::DB_SET, kOrigin4,
      std::vector<std::u16string>(
          {u"key1", u"value1",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(
      Type::DB_SET, kOrigin4,
      std::vector<std::u16string>(
          {u"key2", u"value2",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(
      Type::DB_SET, kOrigin4,
      std::vector<std::u16string>(
          {u"key3", u"value3",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(
      Type::DB_SET, kOrigin4,
      std::vector<std::u16string>(
          {u"key4", u"value4",
           TestDatabaseOperationReceiver::SerializeSetBehavior(
               SetBehavior::kDefault)}));
  operation_list.emplace(Type::DB_LENGTH, kOrigin4);

  operation_list.emplace(Type::DB_FETCH_ORIGINS);

  base::Time override_time =
      base::Time::Now() - base::Days(kStalenessThresholdDays + 1);
  operation_list.emplace(
      Type::DB_OVERRIDE_TIME_ENTRY, kOrigin1,
      std::vector<std::u16string>(
          {u"key1",
           TestDatabaseOperationReceiver::SerializeTime(override_time)}));

  operation_list.emplace(Type::DB_PURGE_STALE);

  operation_list.emplace(Type::DB_LENGTH, kOrigin1);
  operation_list.emplace(Type::DB_LENGTH, kOrigin2);
  operation_list.emplace(Type::DB_LENGTH, kOrigin3);
  operation_list.emplace(Type::DB_LENGTH, kOrigin4);
  operation_list.emplace(Type::DB_FETCH_ORIGINS);

  operation_list.emplace(
      Type::DB_OVERRIDE_TIME_ORIGIN, kOrigin3,
      std::vector<std::u16string>(
          {TestDatabaseOperationReceiver::SerializeTime(override_time)}));
  operation_list.emplace(Type::DB_GET_CREATION_TIME, kOrigin3);
  operation_list.emplace(
      Type::DB_OVERRIDE_TIME_ENTRY, kOrigin3,
      std::vector<std::u16string>(
          {u"key1",
           TestDatabaseOperationReceiver::SerializeTime(override_time)}));
  operation_list.emplace(
      Type::DB_OVERRIDE_TIME_ENTRY, kOrigin3,
      std::vector<std::u16string>(
          {u"key2",
           TestDatabaseOperationReceiver::SerializeTime(override_time)}));
  operation_list.emplace(
      Type::DB_OVERRIDE_TIME_ENTRY, kOrigin3,
      std::vector<std::u16string>(
          {u"key3",
           TestDatabaseOperationReceiver::SerializeTime(override_time)}));
  operation_list.emplace(
      Type::DB_OVERRIDE_TIME_ENTRY, kOrigin1,
      std::vector<std::u16string>(
          {u"key2",
           TestDatabaseOperationReceiver::SerializeTime(override_time)}));
  operation_list.emplace(Type::DB_PURGE_STALE);

  operation_list.emplace(Type::DB_LENGTH, kOrigin1);
  operation_list.emplace(Type::DB_LENGTH, kOrigin2);
  operation_list.emplace(Type::DB_LENGTH, kOrigin3);
  operation_list.emplace(Type::DB_LENGTH, kOrigin4);

  operation_list.emplace(Type::DB_TRIM_MEMORY);
  operation_list.emplace(Type::DB_FETCH_ORIGINS);

  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  operation_list.emplace(
      Type::DB_ENTRIES, kOrigin2,
      std::vector<std::u16string>({base::NumberToString16(id1)}));
  size_t id2 = listener_utility.RegisterListener();
  operation_list.emplace(
      Type::DB_ENTRIES, kOrigin4,
      std::vector<std::u16string>({base::NumberToString16(id2)}));

  SetExpectedOperationList(std::move(operation_list));

  // Check that origin list is initially empty due to the database not being
  // initialized.
  std::vector<mojom::StorageUsageInfoPtr> infos1;
  FetchOrigins(&infos1);

  // Check that calling `PurgeStale()` on the uninitialized database
  // doesn't give an error.
  bool open1 = false;
  IsOpen(&open1);
  InitStatus status1 = InitStatus::kUnattempted;
  DBStatus(&status1);

  OperationResult result1 = OperationResult::kSqlError;
  PurgeStale(&result1);

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
  OverrideLastUsedTime(kOrigin1, u"key1", override_time, &success1);

  OperationResult result12 = OperationResult::kSqlError;
  PurgeStale(&result12);

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
  OverrideCreationTime(kOrigin3, override_time, &success2);

  TimeResult time_result;
  GetCreationTime(kOrigin3, &time_result);

  bool success3 = false;
  OverrideLastUsedTime(kOrigin3, u"key1", override_time, &success3);
  bool success4 = false;
  OverrideLastUsedTime(kOrigin3, u"key2", override_time, &success4);
  bool success5 = false;
  OverrideLastUsedTime(kOrigin3, u"key3", override_time, &success5);
  bool success6 = false;
  OverrideLastUsedTime(kOrigin1, u"key2", override_time, &success6);

  OperationResult result13 = OperationResult::kSqlError;
  PurgeStale(&result13);

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

  OperationResult result14 = OperationResult::kSqlError;
  Entries(kOrigin2, &listener_utility, id1, &result14);
  OperationResult result15 = OperationResult::kSqlError;
  Entries(kOrigin4, &listener_utility, id2, &result15);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  // Database is not yet initialized. `FetchOrigins()` returns an empty vector.
  EXPECT_TRUE(infos1.empty());
  EXPECT_EQ(!GetParam().in_memory_only, open1);
  EXPECT_EQ(InitStatus::kUnattempted, status1);

  // No error from calling `PurgeStale()` on an uninitialized
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
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_TRUE(success1);
  EXPECT_EQ(OperationResult::kSuccess, result12);

  // `kOrigin1` has 1 key cleared. The other origins are not modified.
  EXPECT_EQ(1, length5);
  EXPECT_EQ(1, length6);
  EXPECT_EQ(3, length7);
  EXPECT_EQ(4, length8);

  origins.clear();
  for (const auto& info : infos3)
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_TRUE(success2);
  EXPECT_EQ(override_time, time_result.time);

  EXPECT_TRUE(success3);
  EXPECT_TRUE(success4);
  EXPECT_TRUE(success5);
  EXPECT_TRUE(success6);

  EXPECT_EQ(OperationResult::kSuccess, result13);

  // `kOrigin1` and `kOrigin3` are cleared. The remaining ones are not modified.
  EXPECT_EQ(0, length9);
  EXPECT_EQ(1, length10);
  EXPECT_EQ(0, length11);
  EXPECT_EQ(4, length12);

  origins.clear();
  for (const auto& info : infos4) {
    origins.push_back(info->storage_key.origin());
  }
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin4));

  // Database is still intact after trimming memory.
  EXPECT_EQ(OperationResult::kSuccess, result14);
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id1),
              ElementsAre(std::make_pair(u"key1", u"value1")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);

  EXPECT_EQ(OperationResult::kSuccess, result15);
  listener_utility.FlushForId(id2);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id2),
              ElementsAre(std::make_pair(u"key1", u"value1"),
                          std::make_pair(u"key2", u"value2"),
                          std::make_pair(u"key3", u"value3"),
                          std::make_pair(u"key4", u"value4")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);
}

class AsyncSharedStorageDatabaseImplPurgeMatchingOriginsParamTest
    : public AsyncSharedStorageDatabaseImplTest,
      public testing::WithParamInterface<PurgeMatchingOriginsParams> {
 public:
  DBType GetType() override {
    return GetParam().in_memory_only ? DBType::kInMemory
                                     : DBType::kFileBackedFromNew;
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageBytesPerOrigin",
          base::NumberToString(kMaxBytesPerOrigin)}});
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AsyncSharedStorageDatabaseImplPurgeMatchingOriginsParamTest,
    testing::ValuesIn(GetPurgeMatchingOriginsParams()),
    testing::PrintToStringParamName());

TEST_P(AsyncSharedStorageDatabaseImplPurgeMatchingOriginsParamTest,
       SinceThreshold) {
  // Create keys for two origins. Origin1 will create keys before the expiration
  // time, and Origin2 will create keys after the expiration time. After purge,
  // only Origin2 should have been deleted.
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));

  StorageKeyPolicyMatcherFunctionUtility matcher_utility;

  // Check that the db is unopened.
  {
    base::test::TestFuture<bool> future;
    GetImpl()->IsOpenForTesting(future.GetCallback());
    EXPECT_EQ(false, future.Get());
  }
  {
    base::test::TestFuture<InitStatus> future;
    GetImpl()->DBStatusForTesting(future.GetCallback());
    EXPECT_EQ(InitStatus::kUnattempted, future.Get());
  }

  size_t matcher_id =
      matcher_utility.RegisterMatcherFunction({kOrigin1, kOrigin2});

  // Running purge on an unopened db is fine.
  {
    base::test::TestFuture<OperationResult> future;
    GetImpl()->PurgeMatchingOrigins(
        matcher_utility.TakeMatcherFunctionForId(matcher_id), base::Time::Now(),
        base::Time::Max(), future.GetCallback(),
        GetParam().perform_storage_cleanup);
    EXPECT_EQ(OperationResult::kSuccess, future.Get());
  }

  // Add a key for origin1.
  {
    base::test::TestFuture<OperationResult> future;
    GetImpl()->Set(kOrigin1, u"key1", u"value1", future.GetCallback());
    EXPECT_EQ(OperationResult::kSet, future.Get());
  }

  // Ideally this would use an injected clock, but a 1ms sleep isn't terrible.
  base::PlatformThread::Sleep(base::Milliseconds(1));
  base::Time start_time = base::Time::Now();

  // Now that we're in the future, add a key for origin2.
  {
    base::test::TestFuture<OperationResult> future;
    GetImpl()->Set(kOrigin2, u"key1", u"value1", future.GetCallback());
    EXPECT_EQ(OperationResult::kSet, future.Get());
  }

  // Each origin should have 1 key.
  {
    base::test::TestFuture<int> future;
    GetImpl()->Length(kOrigin1, future.GetCallback());
    EXPECT_EQ(1, future.Get());
  }
  {
    base::test::TestFuture<int> future;
    GetImpl()->Length(kOrigin2, future.GetCallback());
    EXPECT_EQ(1, future.Get());
  }

  // Purge newer stuff, origin2 should be deleted.
  {
    base::test::TestFuture<OperationResult> future;
    GetImpl()->PurgeMatchingOrigins(
        matcher_utility.TakeMatcherFunctionForId(matcher_id), start_time,
        base::Time::Max(), future.GetCallback(),
        GetParam().perform_storage_cleanup);
    EXPECT_EQ(OperationResult::kSuccess, future.Get());
  }

  // Only origin1 should have a key.
  {
    base::test::TestFuture<int> future;
    GetImpl()->Length(kOrigin1, future.GetCallback());
    EXPECT_EQ(1, future.Get());
  }
  {
    base::test::TestFuture<int> future;
    GetImpl()->Length(kOrigin2, future.GetCallback());
    EXPECT_EQ(0, future.Get());
  }
}

}  // namespace storage
