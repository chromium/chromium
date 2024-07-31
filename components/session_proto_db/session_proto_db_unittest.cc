// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/session_proto_db/session_proto_db.h"

#include <map>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/session_proto_db/session_proto_db_test_proto.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using KeyValuePair =
    std::pair<std::string, persisted_state_db::PersistedStateContentProto>;

namespace {
persisted_state_db::PersistedStateContentProto BuildProto(
    const char* protoKey,
    const std::vector<uint8_t> byteArray) {
  persisted_state_db::PersistedStateContentProto proto;
  proto.set_key(protoKey);
  proto.set_content_data(byteArray.data(), byteArray.size());
  return proto;
}

session_proto_db::SessionProtoDBTestProto BuildTestProto(const char* key,
                                                         const int32_t value) {
  session_proto_db::SessionProtoDBTestProto proto;
  proto.set_key(key);
  proto.set_b(value);
  return proto;
}

const char kMockKeyA[] = "A_key";
const char kMockKeyPrefixA[] = "A";
const std::vector<uint8_t> kMockValueArrayA = {0xfa, 0x5b, 0x4c, 0x12};
const persisted_state_db::PersistedStateContentProto kMockValueA =
    BuildProto(kMockKeyA, kMockValueArrayA);
const std::vector<
    SessionProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kExpectedA = {{kMockKeyA, kMockValueA}};
const char kMockKeyB[] = "B_key";
const std::vector<uint8_t> kMockValueArrayB = {0x3c, 0x9f, 0x5e, 0x69};
const persisted_state_db::PersistedStateContentProto kMockValueB =
    BuildProto(kMockKeyB, kMockValueArrayB);
const std::vector<
    SessionProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kExpectedB = {{kMockKeyB, kMockValueB}};
const std::vector<
    SessionProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kExpectedAB = {{kMockKeyA, kMockValueA}, {kMockKeyB, kMockValueB}};
const std::vector<
    SessionProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kEmptyExpected = {};
const session_proto_db::SessionProtoDBTestProto kTestProto =
    BuildTestProto(kMockKeyA, 42);
const std::vector<
    SessionProtoDB<session_proto_db::SessionProtoDBTestProto>::KeyAndValue>
    kTestProtoExpected = {{kMockKeyA, kTestProto}};

const char kSPTDTab1Key[] = "1_SPTD";
const persisted_state_db::PersistedStateContentProto kSPTDTab1Value =
    BuildProto(kSPTDTab1Key, {0x5b, 0x2c});

const char kSPTDTab2Key[] = "2_SPTD";
const persisted_state_db::PersistedStateContentProto kSPTDTab2Value =
    BuildProto(kSPTDTab2Key, {0xfe, 0xab});

const char kSPTDTab3Key[] = "3_SPTD";
const persisted_state_db::PersistedStateContentProto kSPTDTab3Value =
    BuildProto(kSPTDTab3Key, {0x0a, 0x1b});

const char kMPTDTab1Key[] = "1_MPTD";
const persisted_state_db::PersistedStateContentProto kMPTDTab1Value =
    BuildProto(kMPTDTab1Key, {0xaa, 0x3b});

const char kMatchingDataIdForMaintenance[] = "SPTD";
const char kNonMatchingDataIdForMaintenance[] = "asdf";

}  // namespace

class SessionProtoDBTest : public testing::Test {
 public:
  SessionProtoDBTest() = default;
  SessionProtoDBTest(const SessionProtoDBTest&) = delete;
  SessionProtoDBTest& operator=(const SessionProtoDBTest&) = delete;

  // The following methods are specific to the database containing
  // persisted_state_db::PersistedStateContentProto. There is one test which
  // contains session_proto_db::SessionProtoDBTestProto to test the use case
  // of multiple SessionProtoDB databases running at the same time.

  // Initialize the test database
  void InitPersistedStateDB() {
    InitPersistedStateDBWithoutCallback();
    MockInitCallbackPersistedStateDB(content_db_,
                                     leveldb_proto::Enums::InitStatus::kOK);
  }

  void InitPersistedStateDBWithoutCallback() {
    auto storage_db = std::make_unique<leveldb_proto::test::FakeDB<
        persisted_state_db::PersistedStateContentProto>>(&content_db_storage_);
    content_db_ = storage_db.get();
    persisted_state_db_ = base::WrapUnique(
        new SessionProtoDB<persisted_state_db::PersistedStateContentProto>(
            std::move(storage_db),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
            content::GetUIThreadTaskRunner({})));
  }

  void MockInitCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      leveldb_proto::Enums::InitStatus status) {
    storage_db->InitStatusCallback(status);
    RunUntilIdle();
  }

  void MockInsertCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool result) {
    storage_db->UpdateCallback(result);
    RunUntilIdle();
  }

  void MockLoadCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool res) {
    storage_db->LoadCallback(res);
    RunUntilIdle();
  }

  void MockDeleteCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool res) {
    storage_db->UpdateCallback(res);
    RunUntilIdle();
  }

  void GetEvaluationPersistedStateDB(
      base::OnceClosure closure,
      std::vector<SessionProtoDB<
          persisted_state_db::PersistedStateContentProto>::KeyAndValue>
          expected,
      bool result,
      std::vector<SessionProtoDB<
          persisted_state_db::PersistedStateContentProto>::KeyAndValue> found) {
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.content_data(),
                expected[i].second.content_data());
    }
    std::move(closure).Run();
  }

  void GetEmptyEvaluationPersistedStateDB(
      base::OnceClosure closure,
      bool result,
      std::vector<SessionProtoDB<
          persisted_state_db::PersistedStateContentProto>::KeyAndValue> found) {
    EXPECT_TRUE(result);
    EXPECT_EQ(found.size(), 0U);
    std::move(closure).Run();
  }

  // Common to both databases
  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  // Wait for all tasks to be cleared off the queue
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Specific to session_proto_db::SessionProtoDBTestProto database
  void InitTestProtoDB() {
    auto storage_db = std::make_unique<
        leveldb_proto::test::FakeDB<session_proto_db::SessionProtoDBTestProto>>(
        &test_content_db_storage_);
    test_content_db_ = storage_db.get();
    test_proto_db_ = base::WrapUnique(
        new SessionProtoDB<session_proto_db::SessionProtoDBTestProto>(
            std::move(storage_db),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
            content::GetUIThreadTaskRunner({})));
  }

  void GetTestEvaluationTestProtoDB(
      base::OnceClosure closure,
      std::vector<SessionProtoDB<
          session_proto_db::SessionProtoDBTestProto>::KeyAndValue> expected,
      bool result,
      std::vector<SessionProtoDB<
          session_proto_db::SessionProtoDBTestProto>::KeyAndValue> found) {
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.b(), expected[i].second.b());
    }
    std::move(closure).Run();
  }

  void InsertContentAndVerify(
      const std::string& key,
      const persisted_state_db::PersistedStateContentProto& value) {
    InsertContent(key, value);
    VerifyContent(key, value);
  }

  void InsertContent(
      const std::string& key,
      const persisted_state_db::PersistedStateContentProto& value) {
    base::RunLoop wait_for_insert;
    persisted_state_db()->InsertContent(
        key, value,
        base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                       base::Unretained(this), wait_for_insert.QuitClosure(),
                       true));
    MockInsertCallbackPersistedStateDB(content_db(), true);
    wait_for_insert.Run();
  }

  void VerifyContent(
      const std::string& key,
      const persisted_state_db::PersistedStateContentProto& value) {
    base::RunLoop wait_for_load;
    persisted_state_db()->LoadContentWithPrefix(
        key,
        base::BindOnce(
            &SessionProtoDBTest::GetEvaluationPersistedStateDB,
            base::Unretained(this), wait_for_load.QuitClosure(),
            std::vector<SessionProtoDB<
                persisted_state_db::PersistedStateContentProto>::KeyAndValue>(
                {{key, value}})));
    MockLoadCallbackPersistedStateDB(content_db(), true);
    wait_for_load.Run();
  }

  void VerifyContentEmpty(const std::string& key) {
    base::RunLoop wait_for_load;
    persisted_state_db()->LoadContentWithPrefix(
        key,
        base::BindOnce(&SessionProtoDBTest::GetEmptyEvaluationPersistedStateDB,
                       base::Unretained(this), wait_for_load.QuitClosure()));
    MockLoadCallbackPersistedStateDB(content_db(), true);
    wait_for_load.Run();
  }

  void PerformMaintenance(const std::vector<std::string>& keys_to_keep,
                          const std::string& data_id) {
    base::RunLoop wait_for_maintenance;
    persisted_state_db()->PerformMaintenance(
        keys_to_keep, data_id,
        base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                       base::Unretained(this),
                       wait_for_maintenance.QuitClosure(), true));
    MockLoadCallbackPersistedStateDB(content_db(), true);
    MockInsertCallbackPersistedStateDB(content_db(), true);
    wait_for_maintenance.Run();
  }

  // For persisted_state_db::PersistedStateContentProto database
  SessionProtoDB<persisted_state_db::PersistedStateContentProto>*
  persisted_state_db() {
    return persisted_state_db_.get();
  }
  leveldb_proto::test::FakeDB<persisted_state_db::PersistedStateContentProto>*
  content_db() {
    return content_db_;
  }

  std::vector<base::OnceClosure>& deferred_operations() {
    return persisted_state_db()->deferred_operations_;
  }

  bool InitStatusUnknown() { return persisted_state_db()->InitStatusUnknown(); }
  bool FailedToInit() { return persisted_state_db()->FailedToInit(); }

  std::map<std::string, persisted_state_db::PersistedStateContentProto>
      content_db_storage_;

  // For session_proto_db::SessionProtoDBTestProto database
  SessionProtoDB<session_proto_db::SessionProtoDBTestProto>* test_proto_db() {
    return test_proto_db_.get();
  }
  leveldb_proto::test::FakeDB<session_proto_db::SessionProtoDBTestProto>*
  test_content_db() {
    return test_content_db_;
  }

  std::map<std::string, session_proto_db::SessionProtoDBTestProto>
      test_content_db_storage_;

 protected:
  raw_ptr<
      leveldb_proto::test::FakeDB<session_proto_db::SessionProtoDBTestProto>,
      DanglingUntriaged>
      test_content_db_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  // For persisted_state_db::PersistedStateContentProto database
  raw_ptr<leveldb_proto::test::FakeDB<
              persisted_state_db::PersistedStateContentProto>,
          DanglingUntriaged>
      content_db_;
  std::unique_ptr<
      SessionProtoDB<persisted_state_db::PersistedStateContentProto>>
      persisted_state_db_;

  // For session_proto_db::SessionProtoDBTestProto database
  std::unique_ptr<SessionProtoDB<session_proto_db::SessionProtoDBTestProto>>
      test_proto_db_;
};

// Test an arbitrary proto - this ensures we can have two ProfilProtoDB
// databases running simultaneously.
TEST_F(SessionProtoDBTest, TestArbitraryProto) {
  InitTestProtoDB();
  test_content_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  RunUntilIdle();
  base::RunLoop run_loop[2];
  test_proto_db()->InsertContent(
      kMockKeyA, kTestProto,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  test_content_db()->UpdateCallback(true);
  RunUntilIdle();
  run_loop[0].Run();
  test_proto_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetTestEvaluationTestProtoDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kTestProtoExpected));
  test_content_db()->LoadCallback(true);
  RunUntilIdle();
  run_loop[1].Run();
}

TEST_F(SessionProtoDBTest, TestInit) {
  InitPersistedStateDB();
  EXPECT_EQ(false, FailedToInit());
}

TEST_F(SessionProtoDBTest, TestKeyInsertionSucceeded) {
  InitPersistedStateDB();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
}

TEST_F(SessionProtoDBTest, TestKeyInsertionFailed) {
  InitPersistedStateDB();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  MockInsertCallbackPersistedStateDB(content_db(), false);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kEmptyExpected));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
}

TEST_F(SessionProtoDBTest, TestKeyInsertionPrefix) {
  InitPersistedStateDB();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyPrefixA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
}

TEST_F(SessionProtoDBTest, TestLoadOneEntry) {
  InitPersistedStateDB();
  base::RunLoop run_loop[4];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->InsertContent(
      kMockKeyB, kMockValueB,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
  persisted_state_db()->LoadOneEntry(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     kExpectedA));
  content_db()->GetCallback(true);
  run_loop[2].Run();
  persisted_state_db()->LoadOneEntry(
      kMockKeyB,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kExpectedB));
  content_db()->GetCallback(true);
  run_loop[3].Run();
}

TEST_F(SessionProtoDBTest, TestLoadAllEntries) {
  InitPersistedStateDB();
  base::RunLoop run_loop[3];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->InsertContent(
      kMockKeyB, kMockValueB,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
  persisted_state_db()->LoadAllEntries(base::BindOnce(
      &SessionProtoDBTest::GetEvaluationPersistedStateDB,
      base::Unretained(this), run_loop[2].QuitClosure(), kExpectedAB));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[2].Run();
}

TEST_F(SessionProtoDBTest, TestDeleteWithPrefix) {
  InitPersistedStateDB();
  base::RunLoop run_loop[4];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();

  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyPrefixA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  MockDeleteCallbackPersistedStateDB(content_db(), true);
  run_loop[2].Run();

  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kEmptyExpected));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[3].Run();
}

TEST_F(SessionProtoDBTest, TestDeleteOneEntry) {
  InitPersistedStateDB();
  base::RunLoop run_loop[6];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
  persisted_state_db()->DeleteOneEntry(
      kMockKeyPrefixA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), false));
  MockDeleteCallbackPersistedStateDB(content_db(), false);
  run_loop[2].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[3].Run();
  persisted_state_db()->DeleteOneEntry(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  MockDeleteCallbackPersistedStateDB(content_db(), true);
  run_loop[4].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyPrefixA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[5].QuitClosure(),
                     kEmptyExpected));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[5].Run();
}

TEST_F(SessionProtoDBTest, TestDeferredOperations) {
  InitPersistedStateDBWithoutCallback();
  RunUntilIdle();
  EXPECT_EQ(true, InitStatusUnknown());
  base::RunLoop run_loop[4];

  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  EXPECT_EQ(2u, deferred_operations().size());

  content_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_EQ(false, FailedToInit());

  MockInsertCallbackPersistedStateDB(content_db(), true);
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  run_loop[1].Run();
  EXPECT_EQ(0u, deferred_operations().size());

  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  EXPECT_EQ(0u, deferred_operations().size());
  MockDeleteCallbackPersistedStateDB(content_db(), true);

  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kEmptyExpected));
  EXPECT_EQ(0u, deferred_operations().size());
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[3].Run();
}

TEST_F(SessionProtoDBTest, TestInitializationFailure) {
  InitPersistedStateDBWithoutCallback();
  RunUntilIdle();
  EXPECT_EQ(true, InitStatusUnknown());
  base::RunLoop run_loop[6];

  // Do some operations before database status is known
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kEmptyExpected));
  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), false));
  EXPECT_EQ(3u, deferred_operations().size());

  // Error initializing database
  content_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
  EXPECT_EQ(true, FailedToInit());
  for (int i = 0; i < 3; i++) {
    run_loop[i].Run();
  }

  // Check deferred_operations is flushed
  EXPECT_EQ(0u, deferred_operations().size());

  // More operations should just return false/null as the database
  // failed to initialize
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(), false));
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[4].QuitClosure(),
                     kEmptyExpected));
  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));

  // Operations should have returned immediately as database was initialization
  // resulted in an error
  EXPECT_EQ(0u, deferred_operations().size());
  for (int i = 3; i < 6; i++) {
    run_loop[i].Run();
  }
}

TEST_F(SessionProtoDBTest, TestUpdateEntries) {
  InitPersistedStateDB();
  base::RunLoop run_loop[6];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();

  // Do one update and one insert for the UpdateEntries call.
  auto entries_to_update = std::make_unique<std::vector<KeyValuePair>>();
  entries_to_update->emplace_back(kMockKeyA, kMockValueB);
  entries_to_update->emplace_back(kMockKeyB, kMockValueA);
  persisted_state_db()->UpdateEntries(
      std::move(entries_to_update),
      std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();

  persisted_state_db()->LoadOneEntry(
      kMockKeyA,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     kExpectedB));
  content_db()->GetCallback(true);
  run_loop[2].Run();
  persisted_state_db()->LoadOneEntry(
      kMockKeyB,
      base::BindOnce(&SessionProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kExpectedA));
  content_db()->GetCallback(true);
  run_loop[3].Run();

  // Reverts the update and insertion earlier.
  entries_to_update = std::make_unique<std::vector<KeyValuePair>>();
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  entries_to_update->emplace_back(kMockKeyA, kMockValueA);
  keys_to_remove->emplace_back(kMockKeyB);
  persisted_state_db()->UpdateEntries(
      std::move(entries_to_update), std::move(keys_to_remove),
      base::BindOnce(&SessionProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[4].Run();

  persisted_state_db()->LoadAllEntries(base::BindOnce(
      &SessionProtoDBTest::GetEvaluationPersistedStateDB,
      base::Unretained(this), run_loop[5].QuitClosure(), kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
}

TEST_F(SessionProtoDBTest, TestMaintenanceKeepSomeKeys) {
  InitPersistedStateDB();
  InsertContentAndVerify(kSPTDTab1Key, kSPTDTab1Value);
  InsertContentAndVerify(kSPTDTab2Key, kSPTDTab2Value);
  InsertContentAndVerify(kSPTDTab3Key, kSPTDTab3Value);
  InsertContentAndVerify(kMPTDTab1Key, kMPTDTab1Value);
  PerformMaintenance(std::vector<std::string>{kSPTDTab1Key, kSPTDTab3Key},
                     kMatchingDataIdForMaintenance);
  VerifyContent(kSPTDTab1Key, kSPTDTab1Value);
  VerifyContent(kSPTDTab3Key, kSPTDTab3Value);
  VerifyContent(kMPTDTab1Key, kMPTDTab1Value);
  VerifyContentEmpty(kSPTDTab2Key);
}

TEST_F(SessionProtoDBTest, TestMaintenanceKeepNoKeys) {
  InitPersistedStateDB();
  InsertContentAndVerify(kSPTDTab1Key, kSPTDTab1Value);
  InsertContentAndVerify(kSPTDTab2Key, kSPTDTab2Value);
  InsertContentAndVerify(kSPTDTab3Key, kSPTDTab3Value);
  InsertContentAndVerify(kMPTDTab1Key, kMPTDTab1Value);
  PerformMaintenance(std::vector<std::string>{}, kMatchingDataIdForMaintenance);
  VerifyContent(kMPTDTab1Key, kMPTDTab1Value);
  VerifyContentEmpty(kSPTDTab1Key);
  VerifyContentEmpty(kSPTDTab2Key);
  VerifyContentEmpty(kSPTDTab3Key);
}

TEST_F(SessionProtoDBTest, TestMaintenanceNonMatchingDataId) {
  InitPersistedStateDB();
  InsertContentAndVerify(kSPTDTab1Key, kSPTDTab1Value);
  InsertContentAndVerify(kSPTDTab2Key, kSPTDTab2Value);
  InsertContentAndVerify(kSPTDTab3Key, kSPTDTab3Value);
  InsertContentAndVerify(kMPTDTab1Key, kMPTDTab1Value);
  PerformMaintenance(std::vector<std::string>{kSPTDTab1Key, kSPTDTab3Key},
                     kNonMatchingDataIdForMaintenance);
  VerifyContent(kSPTDTab1Key, kSPTDTab1Value);
  VerifyContent(kSPTDTab2Key, kSPTDTab2Value);
  VerifyContent(kSPTDTab3Key, kSPTDTab3Value);
  VerifyContent(kMPTDTab1Key, kMPTDTab1Value);
}
