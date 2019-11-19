// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class SharedProtoDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    temp_dir_ = std::make_unique<base::ScopedTempDir>();
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    db_ = base::WrapRefCounted(
        new SharedProtoDatabase("client", temp_dir_->GetPath()));
  }

  void TearDown() override {}

  void InitDB(bool create_if_missing,
              const std::string& client_name,
              SharedClientInitCallback callback) {
    db_->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedProtoDatabase::Init, db_, create_if_missing,
                       client_name, std::move(callback),
                       task_environment_.GetMainThreadTaskRunner()));
  }

  void KillDB() { db_.reset(); }

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

  SharedProtoDatabase* db() { return db_.get(); }
  ProtoLevelDBWrapper* wrapper() { return db_->db_wrapper_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  scoped_refptr<SharedProtoDatabase> db_;
};

TEST_F(SharedProtoDatabaseTest, CreateClient_SucceedsWithCreate) {
  auto status = Enums::InitStatus::kError;
  GetClientAndWait(db(), ProtoDbType::TEST_DATABASE0,
                   true /* create_if_missing */, &status);
  ASSERT_EQ(status, Enums::InitStatus::kOK);
}

// TODO(912117): Fix flaky test!
#if !defined(OS_ANDROID)
TEST_F(SharedProtoDatabaseTest, DISABLED_CreateClient_FailsWithoutCreate) {
#else
TEST_F(SharedProtoDatabaseTest, CreateClient_FailsWithoutCreate) {
#endif
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
      FROM_HERE, quit_cooldown.QuitClosure(), base::TimeDelta::FromSeconds(3));
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

}  // namespace leveldb_proto
