// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_

#include <deque>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace storage {

using StorageKeyPolicyMatcherFunction =
    SharedStorageDatabase::StorageKeyPolicyMatcherFunction;
using InitStatus = SharedStorageDatabase::InitStatus;
using SetBehavior = SharedStorageDatabase::SetBehavior;
using OperationResult = SharedStorageDatabase::OperationResult;
using GetResult = SharedStorageDatabase::GetResult;
using BudgetResult = SharedStorageDatabase::BudgetResult;
using TimeResult = SharedStorageDatabase::TimeResult;
using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

// For categorizing test databases.
enum class SharedStorageTestDBType {
  kInMemory = 0,
  kFileBackedFromNew = 1,
  kFileBackedFromExisting = 2,
};

// Helper class for testing async operations, accessible here for unit tests
// of both `AsyncSharedStorageDatabase` and `SharedStorageManager`.
class TestDatabaseOperationReceiver {
 public:
  struct DBOperation {
    enum class Type {
      DB_DESTROY = 0,
      DB_TRIM_MEMORY = 1,
      DB_ON_MEMORY_PRESSURE = 2,
      DB_GET = 3,
      DB_SET = 4,
      DB_APPEND = 5,
      DB_DELETE = 6,
      DB_CLEAR = 7,
      DB_LENGTH = 8,
      DB_KEYS = 9,
      DB_ENTRIES = 10,
      DB_PURGE_MATCHING = 11,
      DB_PURGE_STALE = 12,
      DB_FETCH_ORIGINS = 13,
      DB_MAKE_BUDGET_WITHDRAWAL = 14,
      DB_GET_REMAINING_BUDGET = 15,
      DB_IS_OPEN = 16,
      DB_STATUS = 17,
      DB_OVERRIDE_TIME_ORIGIN = 18,
      DB_OVERRIDE_TIME_ENTRY = 19,
      DB_GET_NUM_BUDGET = 20,
      DB_GET_TOTAL_NUM_BUDGET = 21,
      DB_GET_CREATION_TIME = 22,
    } type;
    url::Origin origin;
    std::vector<std::u16string> params;
    explicit DBOperation(Type type);
    DBOperation(Type type, url::Origin origin);
    DBOperation(Type type, net::SchemefulSite site);
    DBOperation(Type type,
                url::Origin origin,
                std::vector<std::u16string> params);
    DBOperation(Type type,
                net::SchemefulSite site,
                std::vector<std::u16string> params);
    DBOperation(Type type, std::vector<std::u16string> params);
    DBOperation(const DBOperation&);
    ~DBOperation();
    bool operator==(const DBOperation& operation) const;
    bool operator!=(const DBOperation& operation) const;
    std::string Serialize() const;
  };

  TestDatabaseOperationReceiver();

  ~TestDatabaseOperationReceiver();

  // For serializing parameters to insert into `params` when creating a
  // `DBOperation` struct.
  static std::u16string SerializeTime(base::Time time);
  static std::u16string SerializeTimeDelta(base::TimeDelta delta);
  static std::u16string SerializeBool(bool b);
  static std::u16string SerializeSetBehavior(SetBehavior behavior);
  static std::u16string SerializeMemoryPressureLevel(MemoryPressureLevel level);

  bool is_finished() const { return finished_; }

  void set_expected_operations(std::queue<DBOperation> expected_operations) {
    expected_operations_ = std::move(expected_operations);
  }

  void WaitForOperations();

  void GetResultCallbackBase(const DBOperation& current_operation,
                             GetResult* out_result,
                             GetResult result);
  base::OnceCallback<void(GetResult)> MakeGetResultCallback(
      const DBOperation& current_operation,
      GetResult* out_result);

  void BudgetResultCallbackBase(const DBOperation& current_operation,
                                BudgetResult* out_result,
                                BudgetResult result);
  base::OnceCallback<void(BudgetResult)> MakeBudgetResultCallback(
      const DBOperation& current_operation,
      BudgetResult* out_result);

  void TimeResultCallbackBase(const DBOperation& current_operation,
                              TimeResult* out_result,
                              TimeResult result);
  base::OnceCallback<void(TimeResult)> MakeTimeResultCallback(
      const DBOperation& current_operation,
      TimeResult* out_result);

  void OperationResultCallbackBase(const DBOperation& current_operation,
                                   OperationResult* out_result,
                                   OperationResult result);
  base::OnceCallback<void(OperationResult)> MakeOperationResultCallback(
      const DBOperation& current_operation,
      OperationResult* out_result);

  void IntCallbackBase(const DBOperation& current_operation,
                       int* out_length,
                       int length);
  base::OnceCallback<void(int)> MakeIntCallback(
      const DBOperation& current_operation,
      int* out_length);

  void BoolCallbackBase(const DBOperation& current_operation,
                        bool* out_boolean,
                        bool boolean);
  base::OnceCallback<void(bool)> MakeBoolCallback(
      const DBOperation& current_operation,
      bool* out_boolean);

  void StatusCallbackBase(const DBOperation& current_operation,
                          InitStatus* out_status,
                          InitStatus status);
  base::OnceCallback<void(InitStatus)> MakeStatusCallback(
      const DBOperation& current_operation,
      InitStatus* out_status);

  void InfosCallbackBase(const DBOperation& current_operation,
                         std::vector<mojom::StorageUsageInfoPtr>* out_infos,
                         std::vector<mojom::StorageUsageInfoPtr> infos);
  base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
  MakeInfosCallback(const DBOperation& current_operation,
                    std::vector<mojom::StorageUsageInfoPtr>* out_infos);

  void OnceClosureBase(const DBOperation& current_operation);
  base::OnceClosure MakeOnceClosure(const DBOperation& current_operation);

  void OnceClosureFromClosureBase(const DBOperation& current_operation,
                                  base::OnceClosure callback);
  base::OnceClosure MakeOnceClosureFromClosure(
      const DBOperation& current_operation,
      base::OnceClosure callback);

 private:
  bool ExpectationsMet(const DBOperation& current_operation);
  void Finish();

  base::RunLoop loop_;
  bool finished_ = true;
  std::queue<DBOperation> expected_operations_;
};

class StorageKeyPolicyMatcherFunctionUtility {
 public:
  StorageKeyPolicyMatcherFunctionUtility();
  ~StorageKeyPolicyMatcherFunctionUtility();

  [[nodiscard]] static StorageKeyPolicyMatcherFunction MakeMatcherFunction(
      std::vector<url::Origin> origins_to_match);

  [[nodiscard]] static StorageKeyPolicyMatcherFunction MakeMatcherFunction(
      std::vector<std::string> origin_strs_to_match);

  [[nodiscard]] size_t RegisterMatcherFunction(
      std::vector<url::Origin> origins_to_match);

  [[nodiscard]] StorageKeyPolicyMatcherFunction TakeMatcherFunctionForId(
      size_t id);

  [[nodiscard]] bool is_empty() const { return matcher_table_.empty(); }

  [[nodiscard]] size_t size() const { return matcher_table_.size(); }

 private:
  std::vector<StorageKeyPolicyMatcherFunction> matcher_table_;
};

class TestSharedStorageEntriesListener
    : public blink::mojom::SharedStorageEntriesListener {
 public:
  explicit TestSharedStorageEntriesListener(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~TestSharedStorageEntriesListener() override;

  void DidReadEntries(
      bool success,
      const std::string& error_message,
      std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> entries,
      bool has_more_entries,
      int total_queued_to_send) override;

  [[nodiscard]] mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
  BindNewPipeAndPassRemote();

  void Flush();

  void VerifyNoError() const;

  [[nodiscard]] std::string error_message() const { return error_message_; }

  [[nodiscard]] size_t BatchCount() const;

  [[nodiscard]] std::vector<std::u16string> TakeKeys();

  [[nodiscard]] std::vector<std::pair<std::u16string, std::u16string>>
  TakeEntries();

 private:
  mojo::Receiver<blink::mojom::SharedStorageEntriesListener> receiver_{this};
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::string error_message_;
  std::deque<blink::mojom::SharedStorageKeyAndOrValuePtr> entries_;
  std::vector<bool> has_more_;
};

class TestSharedStorageEntriesListenerUtility {
 public:
  explicit TestSharedStorageEntriesListenerUtility(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~TestSharedStorageEntriesListenerUtility();

  [[nodiscard]] size_t RegisterListener();

  [[nodiscard]] mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
  BindNewPipeAndPassRemoteForId(size_t id);

  void FlushForId(size_t id);

  void VerifyNoErrorForId(size_t id);

  [[nodiscard]] std::string ErrorMessageForId(size_t id);

  [[nodiscard]] size_t BatchCountForId(size_t id);

  [[nodiscard]] std::vector<std::u16string> TakeKeysForId(size_t id);

  [[nodiscard]] std::vector<std::pair<std::u16string, std::u16string>>
  TakeEntriesForId(size_t id);

  [[nodiscard]] bool is_empty() const { return listener_table_.empty(); }

  [[nodiscard]] size_t size() const { return listener_table_.size(); }

 private:
  [[nodiscard]] TestSharedStorageEntriesListener* GetListenerForId(size_t id);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::deque<TestSharedStorageEntriesListener> listener_table_;
};

// Wraps a bool indicating if the database is in memory only,
// for the purpose of customizing a `PrintToString()` method below, which will
// be used in the parameterized test names via
// `testing::PrintToStringParamName()`.
struct SharedStorageWrappedBool {
  bool in_memory_only;
};

// Wraps bools indicating if the database is in memory only and if storage
// cleanup should be performed after purging, for the purpose of customizing a
// `PrintToString()` method below, which will be used in the parameterized test
// names via `testing::PrintToStringParamName()`.
struct PurgeMatchingOriginsParams {
  bool in_memory_only;
  bool perform_storage_cleanup;
};

[[nodiscard]] std::vector<SharedStorageWrappedBool>
GetSharedStorageWrappedBools();

// Used by `testing::PrintToStringParamName()`.
[[nodiscard]] std::string PrintToString(const SharedStorageWrappedBool& b);

[[nodiscard]] std::vector<PurgeMatchingOriginsParams>
GetPurgeMatchingOriginsParams();

// Used by testing::PrintToStringParamName().
[[nodiscard]] std::string PrintToString(const PurgeMatchingOriginsParams& p);

// Verify that the up-to-date SQL Shared Storage database has the expected
// tables and columns. Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present. Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifySharedStorageTablesAndColumns(sql::Database& db);

[[nodiscard]] bool GetTestDataSharedStorageDir(base::FilePath* dir);

[[nodiscard]] bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                                         std::string ascii_path);

[[nodiscard]] std::string TimeDeltaToString(base::TimeDelta delta);

[[nodiscard]] BudgetResult MakeBudgetResultForSqlError();

[[nodiscard]] std::string GetTestFileNameForVersion(int version_number);

[[nodiscard]] std::string GetTestFileNameForCurrentVersion();

[[nodiscard]] std::string GetTestFileNameForLatestDeprecatedVersion();

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_
