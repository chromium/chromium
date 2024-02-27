// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_test_utils.h"

#include <deque>
#include <iterator>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {

TestDatabaseOperationReceiver::DBOperation::DBOperation(Type type)
    : type(type) {
  DCHECK(type == Type::DB_IS_OPEN || type == Type::DB_STATUS ||
         type == Type::DB_DESTROY || type == Type::DB_TRIM_MEMORY ||
         type == Type::DB_GET_TOTAL_NUM_BUDGET ||
         type == Type::DB_PURGE_STALE || type == Type::DB_FETCH_ORIGINS);
}

TestDatabaseOperationReceiver::DBOperation::DBOperation(Type type,
                                                        url::Origin origin)
    : type(type), origin(std::move(origin)) {
  DCHECK(type == Type::DB_LENGTH || type == Type::DB_CLEAR ||
         type == Type::DB_GET_CREATION_TIME);
}

TestDatabaseOperationReceiver::DBOperation::DBOperation(Type type,
                                                        net::SchemefulSite site)
    : type(type), origin(site.GetInternalOriginForTesting()) {  // IN-TEST
  DCHECK(type == Type::DB_GET_REMAINING_BUDGET ||
         type == Type::DB_GET_NUM_BUDGET);
}

TestDatabaseOperationReceiver::DBOperation::DBOperation(
    Type type,
    url::Origin origin,
    std::vector<std::u16string> params)
    : type(type), origin(std::move(origin)), params(std::move(params)) {
  DCHECK(type == Type::DB_GET || type == Type::DB_SET ||
         type == Type::DB_APPEND || type == Type::DB_DELETE ||
         type == Type::DB_KEYS || type == Type::DB_ENTRIES ||
         type == Type::DB_OVERRIDE_TIME_ORIGIN ||
         type == Type::DB_OVERRIDE_TIME_ENTRY);
}

TestDatabaseOperationReceiver::DBOperation::DBOperation(
    Type type,
    net::SchemefulSite site,
    std::vector<std::u16string> params)
    : type(type),
      origin(site.GetInternalOriginForTesting()),  // IN-TEST
      params(std::move(params)) {
  DCHECK_EQ(type, Type::DB_MAKE_BUDGET_WITHDRAWAL);
}

TestDatabaseOperationReceiver::DBOperation::DBOperation(
    Type type,
    std::vector<std::u16string> params)
    : type(type), params(std::move(params)) {
  DCHECK(type == Type::DB_ON_MEMORY_PRESSURE ||
         type == Type::DB_PURGE_MATCHING);
}

TestDatabaseOperationReceiver::DBOperation::~DBOperation() = default;

TestDatabaseOperationReceiver::DBOperation::DBOperation(const DBOperation&) =
    default;

bool TestDatabaseOperationReceiver::DBOperation::operator==(
    const DBOperation& operation) const {
  if (type != operation.type || params != operation.params)
    return false;

  if (origin.opaque() && operation.origin.opaque())
    return true;

  return origin == operation.origin;
}

bool TestDatabaseOperationReceiver::DBOperation::operator!=(
    const DBOperation& operation) const {
  if (type != operation.type || params != operation.params)
    return true;

  if (origin.opaque() && operation.origin.opaque())
    return false;

  return origin != operation.origin;
}

std::string TestDatabaseOperationReceiver::DBOperation::Serialize() const {
  std::string serialization(
      base::StrCat({"type: ", base::NumberToString(static_cast<int>(type)),
                    "; origin: ", origin.Serialize(), "; params: {"}));
  for (int i = 0; i < static_cast<int>(params.size()) - 1; i++) {
    serialization =
        base::StrCat({serialization, base::UTF16ToUTF8(params[i]), ","});
  }
  serialization = params.empty()
                      ? base::StrCat({serialization, "}"})
                      : base::StrCat({serialization,
                                      base::UTF16ToUTF8(params.back()), "}"});
  return serialization;
}

TestDatabaseOperationReceiver::TestDatabaseOperationReceiver() = default;

TestDatabaseOperationReceiver::~TestDatabaseOperationReceiver() = default;

// static
std::u16string TestDatabaseOperationReceiver::SerializeTime(base::Time time) {
  return SerializeTimeDelta(time.ToDeltaSinceWindowsEpoch());
}

// static

std::u16string TestDatabaseOperationReceiver::SerializeTimeDelta(
    base::TimeDelta delta) {
  return base::StrCat({base::NumberToString16(delta.InMicroseconds()), u"us"});
}

// static
std::u16string TestDatabaseOperationReceiver::SerializeBool(bool b) {
  return b ? u"true" : u"false";
}

// static
std::u16string TestDatabaseOperationReceiver::SerializeSetBehavior(
    SetBehavior behavior) {
  return base::NumberToString16(static_cast<int>(behavior));
}

// static
std::u16string TestDatabaseOperationReceiver::SerializeMemoryPressureLevel(
    MemoryPressureLevel level) {
  return base::NumberToString16(static_cast<int>(level));
}

void TestDatabaseOperationReceiver::WaitForOperations() {
  finished_ = false;
  loop_.Run();
  if (expected_operations_.empty())
    Finish();
}

void TestDatabaseOperationReceiver::GetResultCallbackBase(
    const DBOperation& current_operation,
    GetResult* out_result,
    GetResult result) {
  DCHECK(out_result);
  *out_result = std::move(result);

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(GetResult)>
TestDatabaseOperationReceiver::MakeGetResultCallback(
    const DBOperation& current_operation,
    GetResult* out_result) {
  return base::BindOnce(&TestDatabaseOperationReceiver::GetResultCallbackBase,
                        base::Unretained(this), current_operation, out_result);
}

void TestDatabaseOperationReceiver::BudgetResultCallbackBase(
    const DBOperation& current_operation,
    BudgetResult* out_result,
    BudgetResult result) {
  DCHECK(out_result);
  *out_result = std::move(result);

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(BudgetResult)>
TestDatabaseOperationReceiver::MakeBudgetResultCallback(
    const DBOperation& current_operation,
    BudgetResult* out_result) {
  return base::BindOnce(
      &TestDatabaseOperationReceiver::BudgetResultCallbackBase,
      base::Unretained(this), current_operation, out_result);
}

void TestDatabaseOperationReceiver::TimeResultCallbackBase(
    const DBOperation& current_operation,
    TimeResult* out_result,
    TimeResult result) {
  DCHECK(out_result);
  *out_result = std::move(result);

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(TimeResult)>
TestDatabaseOperationReceiver::MakeTimeResultCallback(
    const DBOperation& current_operation,
    TimeResult* out_result) {
  return base::BindOnce(&TestDatabaseOperationReceiver::TimeResultCallbackBase,
                        base::Unretained(this), current_operation, out_result);
}

void TestDatabaseOperationReceiver::OperationResultCallbackBase(
    const DBOperation& current_operation,
    OperationResult* out_result,
    OperationResult result) {
  DCHECK(out_result);
  *out_result = result;

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(OperationResult)>
TestDatabaseOperationReceiver::MakeOperationResultCallback(
    const DBOperation& current_operation,
    OperationResult* out_result) {
  return base::BindOnce(
      &TestDatabaseOperationReceiver::OperationResultCallbackBase,
      base::Unretained(this), current_operation, out_result);
}

void TestDatabaseOperationReceiver::IntCallbackBase(
    const DBOperation& current_operation,
    int* out_length,
    int length) {
  DCHECK(out_length);
  *out_length = length;

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(int)> TestDatabaseOperationReceiver::MakeIntCallback(
    const DBOperation& current_operation,
    int* out_length) {
  return base::BindOnce(&TestDatabaseOperationReceiver::IntCallbackBase,
                        base::Unretained(this), current_operation, out_length);
}

void TestDatabaseOperationReceiver::BoolCallbackBase(
    const DBOperation& current_operation,
    bool* out_boolean,
    bool boolean) {
  DCHECK(out_boolean);
  *out_boolean = boolean;

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(bool)> TestDatabaseOperationReceiver::MakeBoolCallback(
    const DBOperation& current_operation,
    bool* out_boolean) {
  return base::BindOnce(&TestDatabaseOperationReceiver::BoolCallbackBase,
                        base::Unretained(this), current_operation, out_boolean);
}

void TestDatabaseOperationReceiver::StatusCallbackBase(
    const DBOperation& current_operation,
    InitStatus* out_status,
    InitStatus status) {
  DCHECK(out_status);
  *out_status = status;

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(InitStatus)>
TestDatabaseOperationReceiver::MakeStatusCallback(
    const DBOperation& current_operation,
    InitStatus* out_status) {
  return base::BindOnce(&TestDatabaseOperationReceiver::StatusCallbackBase,
                        base::Unretained(this), current_operation, out_status);
}

void TestDatabaseOperationReceiver::InfosCallbackBase(
    const DBOperation& current_operation,
    std::vector<mojom::StorageUsageInfoPtr>* out_infos,
    std::vector<mojom::StorageUsageInfoPtr> infos) {
  DCHECK(out_infos);
  *out_infos = std::move(infos);

  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
TestDatabaseOperationReceiver::MakeInfosCallback(
    const DBOperation& current_operation,
    std::vector<mojom::StorageUsageInfoPtr>* out_infos) {
  return base::BindOnce(&TestDatabaseOperationReceiver::InfosCallbackBase,
                        base::Unretained(this), current_operation, out_infos);
}

void TestDatabaseOperationReceiver::OnceClosureBase(
    const DBOperation& current_operation) {
  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
}

base::OnceClosure TestDatabaseOperationReceiver::MakeOnceClosure(
    const DBOperation& current_operation) {
  return base::BindOnce(&TestDatabaseOperationReceiver::OnceClosureBase,
                        base::Unretained(this), current_operation);
}

void TestDatabaseOperationReceiver::OnceClosureFromClosureBase(
    const DBOperation& current_operation,
    base::OnceClosure callback) {
  if (ExpectationsMet(current_operation) && loop_.running())
    Finish();
  if (callback)
    std::move(callback).Run();
}

base::OnceClosure TestDatabaseOperationReceiver::MakeOnceClosureFromClosure(
    const DBOperation& current_operation,
    base::OnceClosure callback) {
  return base::BindOnce(
      &TestDatabaseOperationReceiver::OnceClosureFromClosureBase,
      base::Unretained(this), current_operation, std::move(callback));
}

bool TestDatabaseOperationReceiver::ExpectationsMet(
    const DBOperation& current_operation) {
  EXPECT_FALSE(expected_operations_.empty());

  if (expected_operations_.empty())
    return false;

  EXPECT_EQ(expected_operations_.front(), current_operation)
      << "expected operation: " << expected_operations_.front().Serialize()
      << std::endl
      << "actual operation: " << current_operation.Serialize() << std::endl;

  if (expected_operations_.front() != current_operation) {
    return false;
  } else {
    expected_operations_.pop();
    return expected_operations_.empty();
  }
}

void TestDatabaseOperationReceiver::Finish() {
  finished_ = true;
  loop_.Quit();
}

StorageKeyPolicyMatcherFunctionUtility::
    StorageKeyPolicyMatcherFunctionUtility() = default;
StorageKeyPolicyMatcherFunctionUtility::
    ~StorageKeyPolicyMatcherFunctionUtility() = default;

StorageKeyPolicyMatcherFunction
StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
    std::vector<url::Origin> origins_to_match) {
  return base::BindRepeating(
      [](std::vector<url::Origin> origins_to_match,
         const blink::StorageKey& storage_key, SpecialStoragePolicy* policy) {
        return base::Contains(origins_to_match, storage_key.origin());
      },
      origins_to_match);
}

StorageKeyPolicyMatcherFunction
StorageKeyPolicyMatcherFunctionUtility::MakeMatcherFunction(
    std::vector<std::string> origin_strs_to_match) {
  std::vector<url::Origin> origins_to_match;
  for (const auto& str : origin_strs_to_match)
    origins_to_match.push_back(url::Origin::Create(GURL(str)));
  return MakeMatcherFunction(origins_to_match);
}

size_t StorageKeyPolicyMatcherFunctionUtility::RegisterMatcherFunction(
    std::vector<url::Origin> origins_to_match) {
  matcher_table_.emplace_back(MakeMatcherFunction(origins_to_match));
  return matcher_table_.size() - 1;
}

StorageKeyPolicyMatcherFunction
StorageKeyPolicyMatcherFunctionUtility::TakeMatcherFunctionForId(size_t id) {
  DCHECK_LT(id, matcher_table_.size());
  return std::move(matcher_table_[id]);
}

TestSharedStorageEntriesListener::TestSharedStorageEntriesListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

TestSharedStorageEntriesListener::~TestSharedStorageEntriesListener() = default;

void TestSharedStorageEntriesListener::DidReadEntries(
    bool success,
    const std::string& error_message,
    std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> entries,
    bool has_more_entries,
    int total_queued_to_send) {
  if (!success) {
    error_message_ = error_message;
    return;
  }

  using iter_type =
      std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr>::iterator;
  entries_.insert(entries_.end(),
                  std::move_iterator<iter_type>(entries.begin()),
                  std::move_iterator<iter_type>(entries.end()));
  has_more_.push_back(has_more_entries);
}

mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
TestSharedStorageEntriesListener::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote(task_runner_);
}

void TestSharedStorageEntriesListener::Flush() {
  receiver_.FlushForTesting();
}

void TestSharedStorageEntriesListener::VerifyNoError() const {
  DCHECK(!has_more_.empty());
  for (size_t i = 0; i < has_more_.size() - 1; i++)
    EXPECT_TRUE(has_more_[i]);
  EXPECT_FALSE(has_more_.back());
  EXPECT_TRUE(error_message_.empty());
}

size_t TestSharedStorageEntriesListener::BatchCount() const {
  return has_more_.size();
}

std::vector<std::u16string> TestSharedStorageEntriesListener::TakeKeys() {
  std::vector<std::u16string> keys;
  while (!entries_.empty()) {
    blink::mojom::SharedStorageKeyAndOrValuePtr entry =
        std::move(entries_.front());
    entries_.pop_front();
    keys.emplace_back(std::move(entry->key));
  }
  return keys;
}

std::vector<std::pair<std::u16string, std::u16string>>
TestSharedStorageEntriesListener::TakeEntries() {
  std::vector<std::pair<std::u16string, std::u16string>> entries;
  while (!entries_.empty()) {
    blink::mojom::SharedStorageKeyAndOrValuePtr entry =
        std::move(entries_.front());
    entries_.pop_front();
    entries.emplace_back(std::move(entry->key), std::move(entry->value));
  }
  return entries;
}

TestSharedStorageEntriesListenerUtility::
    TestSharedStorageEntriesListenerUtility(
        scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

TestSharedStorageEntriesListenerUtility::
    ~TestSharedStorageEntriesListenerUtility() = default;

size_t TestSharedStorageEntriesListenerUtility::RegisterListener() {
  listener_table_.emplace_back(task_runner_);
  return listener_table_.size() - 1;
}

mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
TestSharedStorageEntriesListenerUtility::BindNewPipeAndPassRemoteForId(
    size_t id) {
  return GetListenerForId(id)->BindNewPipeAndPassRemote();
}

void TestSharedStorageEntriesListenerUtility::FlushForId(size_t id) {
  GetListenerForId(id)->Flush();
}

void TestSharedStorageEntriesListenerUtility::VerifyNoErrorForId(size_t id) {
  GetListenerForId(id)->VerifyNoError();
}

std::string TestSharedStorageEntriesListenerUtility::ErrorMessageForId(
    size_t id) {
  return GetListenerForId(id)->error_message();
}

size_t TestSharedStorageEntriesListenerUtility::BatchCountForId(size_t id) {
  return GetListenerForId(id)->BatchCount();
}

std::vector<std::u16string>
TestSharedStorageEntriesListenerUtility::TakeKeysForId(size_t id) {
  return GetListenerForId(id)->TakeKeys();
}

std::vector<std::pair<std::u16string, std::u16string>>
TestSharedStorageEntriesListenerUtility::TakeEntriesForId(size_t id) {
  return GetListenerForId(id)->TakeEntries();
}

TestSharedStorageEntriesListener*
TestSharedStorageEntriesListenerUtility::GetListenerForId(size_t id) {
  DCHECK_LT(id, listener_table_.size());
  return &listener_table_[id];
}

std::vector<SharedStorageWrappedBool> GetSharedStorageWrappedBools() {
  return std::vector<SharedStorageWrappedBool>({{true}, {false}});
}

std::string PrintToString(const SharedStorageWrappedBool& b) {
  return b.in_memory_only ? "InMemoryOnly" : "FileBacked";
}

std::vector<PurgeMatchingOriginsParams> GetPurgeMatchingOriginsParams() {
  return std::vector<PurgeMatchingOriginsParams>(
      {{true, true}, {true, false}, {false, true}, {false, false}});
}

std::string PrintToString(const PurgeMatchingOriginsParams& p) {
  return base::StrCat({(p.in_memory_only ? "InMemoryOnly" : "FileBacked"),
                       "_With", (p.perform_storage_cleanup ? "" : "out"),
                       "Cleanup"});
}

void VerifySharedStorageTablesAndColumns(sql::Database& db) {
  // `meta`, `values_mapping`, `per_origin_mapping`, and budget_mapping.
  EXPECT_EQ(4u, sql::test::CountSQLTables(&db));

  // Implicit index on `meta`, `values_mapping_last_used_time_idx`,
  // `per_origin_mapping_creation_time_idx`, and
  // budget_mapping_site_time_stamp_idx.
  EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

  // `key` and `value`.
  EXPECT_EQ(2u, sql::test::CountTableColumns(&db, "meta"));

  // `context_origin`, `key`, `value`, and `last_used_time`.
  EXPECT_EQ(4u, sql::test::CountTableColumns(&db, "values_mapping"));

  // `context_origin`, `creation_time`, and `num_bytes`.
  EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "per_origin_mapping"));

  // `id`, `context_site`, `time_stamp`, and `bits_debit`.
  EXPECT_EQ(4u, sql::test::CountTableColumns(&db, "budget_mapping"));
}

bool GetTestDataSharedStorageDir(base::FilePath* dir) {
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, dir)) {
    return false;
  }
  *dir = dir->AppendASCII("components");
  *dir = dir->AppendASCII("test");
  *dir = dir->AppendASCII("data");
  *dir = dir->AppendASCII("storage");
  return true;
}

bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                           std::string ascii_path) {
  base::FilePath dir;
  if (!GetTestDataSharedStorageDir(&dir))
    return false;
  return sql::test::CreateDatabaseFromSQL(db_path, dir.AppendASCII(ascii_path));
}

std::string TimeDeltaToString(base::TimeDelta delta) {
  return base::StrCat({base::NumberToString(delta.InMilliseconds()), "ms"});
}

BudgetResult MakeBudgetResultForSqlError() {
  return BudgetResult(0.0, OperationResult::kSqlError);
}

std::string GetTestFileNameForVersion(int version_number) {
  // Should be safe cross platform because StringPrintf has overloads for wide
  // strings.
  return base::StringPrintf("shared_storage.v%d.sql", version_number);
}

std::string GetTestFileNameForCurrentVersion() {
  return GetTestFileNameForVersion(
      SharedStorageDatabase::kCurrentVersionNumber);
}

std::string GetTestFileNameForLatestDeprecatedVersion() {
  return GetTestFileNameForVersion(
      SharedStorageDatabase::kDeprecatedVersionNumber);
}

}  // namespace storage
