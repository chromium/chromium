// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace leveldb_proto {

namespace {

inline void GetClientFromTaskRunner(SharedProtoDatabase* db,
                                    ProtoDbType db_type,
                                    base::OnceClosure closure) {
  db->GetClientForTesting(
      db_type, true /* create_if_missing */,
      base::BindOnce(
          [](base::OnceClosure closure, Enums::InitStatus status,
             SharedDBMetadataProto::MigrationStatus migration_status) {
            EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                      migration_status);
            std::move(closure).Run();
          },
          std::move(closure)));
}

}  // namespace

class MockSharedDb : public SharedProtoDatabase {
 public:
  MockSharedDb(const std::string& client_db_id, const base::FilePath& db_dir)
      : SharedProtoDatabase(client_db_id, db_dir) {}
  MOCK_METHOD1(DestroyObsoleteSharedProtoDatabaseClients,
               void(Callbacks::UpdateCallback));

 private:
  friend class base::RefCountedThreadSafe<MockSharedDb>;
  friend class SharedProtoDatabaseTest;

  ~MockSharedDb() override = default;
};

class SharedProtoDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = base::WrapRefCounted(new MockSharedDb("client", temp_dir_.GetPath()));
  }

  void TearDown() override {
    if (db_)
      db_->Shutdown();
  }

  void InitDB(bool create_if_missing,
              const std::string& client_name,
              SharedClientInitCallback callback) {
    db_->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedProtoDatabase::Init, db_, create_if_missing,
                       client_name, std::move(callback),
                       task_environment_.GetMainThreadTaskRunner()));
  }

  void KillDB() {
    db_->Shutdown();
    db_.reset();
  }

  bool IsDatabaseInitialized(SharedProtoDatabase* db) {
    return db->init_state_ == SharedProtoDatabase::InitState::kSuccess;
  }

  std::unique_ptr<SharedProtoDatabaseClient> GetClientAndWait(
      SharedProtoDatabase* db,
      ProtoDbType db_type,
      bool create_if_missing,
      Enums::InitStatus* status) {
    base::RunLoop loop;
    auto client = db->GetClientForTesting(
        db_type, create_if_missing,
        base::BindOnce(
            [](Enums::InitStatus* status_out, base::OnceClosure closure,
               Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                        migration_status);
              *status_out = status;
              std::move(closure).Run();
            },
            status, loop.QuitClosure()));
    loop.Run();
    return client;
  }

  scoped_refptr<base::SequencedTaskRunner> GetMainThreadTaskRunner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

  MockSharedDb* db() { return db_.get(); }
  ProtoLevelDBWrapper* wrapper() { return db_->db_wrapper_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<MockSharedDb> db_;
};

TEST_F(SharedProtoDatabaseTest, CreateClient_SucceedsWithCreate) {
  auto status = Enums::InitStatus::kError;
  GetClientAndWait(db(), ProtoDbType::TEST_DATABASE0,
                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
}

TEST_F(SharedProtoDatabaseTest, CreateClient_FailsWithoutCreate) {
  auto status = Enums::InitStatus::kError;
  GetClientAndWait(db(), ProtoDbType::TEST_DATABASE0,
                   false /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kInvalidOperation);
}

TEST_F(SharedProtoDatabaseTest,
       CreateClient_SucceedsWithoutCreateIfAlreadyCreated) {
  auto status = Enums::InitStatus::kError;
  GetClientAndWait(db(), ProtoDbType::TEST_DATABASE2,
                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
  GetClientAndWait(db(), ProtoDbType::TEST_DATABASE0,
                   false /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
}

TEST_F(SharedProtoDatabaseTest, GetClient_DifferentThreads) {
  auto status = Enums::InitStatus::kError;
  GetClientAndWait(db(), ProtoDbType::TEST_DATABASE0,
                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);

  base::Thread t("test_thread");
  ASSERT_TRUE(t.Start());
  base::RunLoop run_loop;
  t.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetClientFromTaskRunner, base::Unretained(db()),
                     ProtoDbType::TEST_DATABASE2, run_loop.QuitClosure()));
  run_loop.Run();
  base::RunLoop quit_cooldown;
  GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, quit_cooldown.QuitClosure(), base::Seconds(3));
}

// If not attempt to create the db, kInvalidOperation will be returned in the
// callback.
TEST_F(SharedProtoDatabaseTest, InitNotCreateDb) {
  base::RunLoop run_init_loop;
  InitDB(false /* create_if_missing */, "TestDatabaseUMA",
         base::BindOnce(
             [](base::OnceClosure signal, Enums::InitStatus status,
                SharedDBMetadataProto::MigrationStatus migration_status) {
               EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                         migration_status);
               EXPECT_EQ(status, Enums::InitStatus::kInvalidOperation);
               std::move(signal).Run();
             },
             run_init_loop.QuitClosure()));
  run_init_loop.Run();
}

// If two initialize calls with different create_if_missing parameter arrive at
// the same time, the shared db will be created.
TEST_F(SharedProtoDatabaseTest, InitWithDifferentCreateIfMissing) {
  base::RunLoop run_init_loop;
  InitDB(false /* create_if_missing */, "TestDatabaseUMA1",
         base::BindOnce(
             [](base::OnceClosure signal, Enums::InitStatus status,
                SharedDBMetadataProto::MigrationStatus migration_status) {
               EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                         migration_status);
               EXPECT_EQ(status, Enums::InitStatus::kOK);
               std::move(signal).Run();
             },
             run_init_loop.QuitClosure()));

  InitDB(true /* create_if_missing */, "TestDatabaseUMA2",
         base::BindOnce(
             [](Enums::InitStatus status,
                SharedDBMetadataProto::MigrationStatus migration_status) {
               EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                         migration_status);
               EXPECT_EQ(status, Enums::InitStatus::kOK);
             }));

  run_init_loop.Run();
}

// Tests that the shared DB's destructor behaves appropriately once the
// backing LevelDB has been initialized on another thread.
TEST_F(SharedProtoDatabaseTest, TestDBDestructionAfterInit) {
  base::RunLoop run_init_loop;
  InitDB(true /* create_if_missing */, "TestDatabaseUMA",
         base::BindOnce(
             [](base::OnceClosure signal, Enums::InitStatus status,
                SharedDBMetadataProto::MigrationStatus migration_status) {
               EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                         migration_status);
               ASSERT_EQ(status, Enums::InitStatus::kOK);
               std::move(signal).Run();
             },
             run_init_loop.QuitClosure()));
  run_init_loop.Run();
  KillDB();
}

TEST_F(SharedProtoDatabaseTest, CancelDeleteObsoleteClients) {
  base::RunLoop run_init_loop;
  EXPECT_CALL(*db(), DestroyObsoleteSharedProtoDatabaseClients(_)).Times(0);
  InitDB(true /* create_if_missing */, "TestDatabaseUMA",
         base::BindOnce(
             [](base::OnceClosure signal, Enums::InitStatus status,
                SharedDBMetadataProto::MigrationStatus migration_status) {
               EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                         migration_status);
               ASSERT_EQ(status, Enums::InitStatus::kOK);
               std::move(signal).Run();
             },
             run_init_loop.QuitClosure()));
  run_init_loop.Run();

  auto db_task_runner = db()->database_task_runner_for_testing();

  KillDB();

  base::RunLoop wait_task;
  db_task_runner->PostTask(FROM_HERE, wait_task.QuitClosure());
  wait_task.Run();
}

TEST_F(SharedProtoDatabaseTest, DeleteObsoleteClients) {
  db()->SetDeleteObsoleteDelayForTesting(base::TimeDelta());
  EXPECT_CALL(*db(), DestroyObsoleteSharedProtoDatabaseClients(_)).Times(1);
  base::RunLoop run_init_loop;
  InitDB(true /* create_if_missing */, "TestDatabaseUMA",
         base::BindOnce(
             [](base::OnceClosure signal, Enums::InitStatus status,
                SharedDBMetadataProto::MigrationStatus migration_status) {
               EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                         migration_status);
               ASSERT_EQ(status, Enums::InitStatus::kOK);
               std::move(signal).Run();
             },
             run_init_loop.QuitClosure()));
  run_init_loop.Run();
  auto db_task_runner = db()->database_task_runner_for_testing();

  base::RunLoop wait_task;
  db_task_runner->PostTask(FROM_HERE, wait_task.QuitClosure());
  wait_task.Run();

  KillDB();
}

}  // namespace leveldb_proto
