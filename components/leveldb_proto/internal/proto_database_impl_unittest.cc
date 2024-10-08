// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_impl.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"
#include "components/leveldb_proto/internal/proto_database_selector.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

namespace {

const std::string kDefaultClientName = "client";

// Example struct defined by clients that can be used instead of protos.
struct ClientStruct {
 public:
  ClientStruct() = default;
  ClientStruct(ClientStruct&& other) {
    id_ = std::move(other.id_);
    data_ = std::move(other.data_);
  }

  ClientStruct(const ClientStruct&) = delete;
  ClientStruct& operator=(const ClientStruct&) = delete;

  ~ClientStruct() = default;

  // The methods below are convenience methods to have a similar API as protocol
  // buffers for the test framework. This is NOT required for uses of client
  // structs.
  std::string id() const { return id_; }
  std::string data() const { return data_; }

  std::string id_;
  std::string data_;
};

void CreateData(const std::string& key,
                const std::string& data,
                TestProto* proto) {
  // Ensure the DB key, the id-field and data-field are all unique values.
  proto->set_id(key + key);
  proto->set_data(key + key + key);
}

void CreateData(const std::string& key,
                const std::string& data,
                ClientStruct* as_struct) {
  // Ensure the DB key, the id-field and data-field are all unique values.
  as_struct->id_ = key + key;
  as_struct->data_ = key + key + key;
}

void DataToProto(ClientStruct* data, TestProto* proto) {
  proto->mutable_id()->swap(data->id_);
  proto->mutable_data()->swap(data->data_);
}

void ProtoToData(TestProto* proto, ClientStruct* data) {
  proto->mutable_id()->swap(data->id_);
  proto->mutable_data()->swap(data->data_);
}

}  // namespace

// Class used to shortcut the Init method so it returns a specified InitStatus.
class TestSharedProtoDatabase : public SharedProtoDatabase {
 public:
  TestSharedProtoDatabase(const std::string& client_db_id,
                          const base::FilePath& db_dir,
                          Enums::InitStatus use_status)
      : SharedProtoDatabase::SharedProtoDatabase(client_db_id, db_dir) {
    use_status_ = use_status;
  }

  void Init(
      bool create_if_missing,
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    init_status_ = use_status_;

    CheckCorruptionAndRunInitCallback(client_db_id, std::move(callback),
                                      std::move(callback_task_runner),
                                      use_status_);
  }

 private:
  ~TestSharedProtoDatabase() override = default;

  Enums::InitStatus use_status_;
};

class TestSharedProtoDatabaseClient : public SharedProtoDatabaseClient {
 public:
  using SharedProtoDatabaseClient::set_migration_status;
  using SharedProtoDatabaseClient::SharedProtoDatabaseClient;

  explicit TestSharedProtoDatabaseClient(
      scoped_refptr<SharedProtoDatabase> shared_db)
      : SharedProtoDatabaseClient::SharedProtoDatabaseClient(
            std::make_unique<ProtoLevelDBWrapper>(shared_db->task_runner_,
                                                  shared_db->db_.get()),
            ProtoDbType::TEST_DATABASE1,
            shared_db) {}

  void UpdateClientInitMetadata(
      SharedDBMetadataProto::MigrationStatus migration_status) override {
    set_migration_status(migration_status);
  }

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) override {
    std::move(callback).Run(true);
  }
};

class TestProtoDatabaseProvider : public ProtoDatabaseProvider {
 public:
  explicit TestProtoDatabaseProvider(const base::FilePath& profile_dir)
      : ProtoDatabaseProvider(profile_dir) {}
  TestProtoDatabaseProvider(const base::FilePath& profile_dir,
                            const scoped_refptr<SharedProtoDatabase>& shared_db)
      : ProtoDatabaseProvider(profile_dir) {
    shared_db_ = shared_db;
  }

  void GetSharedDBInstance(
      ProtoDatabaseProvider::GetSharedDBInstanceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), shared_db_));
  }

 private:
  scoped_refptr<SharedProtoDatabase> shared_db_;
};

class TestSharedProtoDatabaseProvider : public SharedProtoDatabaseProvider {
 public:
  TestSharedProtoDatabaseProvider(
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      base::WeakPtr<ProtoDatabaseProvider> provider_weak_ptr)
      : SharedProtoDatabaseProvider(std::move(client_task_runner),
                                    std::move(provider_weak_ptr)) {}
};

template <typename T>
class ProtoDatabaseImplTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(shared_db_temp_dir_.CreateUniqueTempDir());
    shared_db_ = base::WrapRefCounted(new SharedProtoDatabase(
        kDefaultClientName, shared_db_temp_dir_.GetPath()));
    test_task_runner_ = shared_db_->database_task_runner_for_testing();
  }

  void TearDown() override { shared_db_->Shutdown(); }

  void SetUpExperimentParams(std::map<std::string, std::string> params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kProtoDBSharedMigration, params);
  }

  std::unique_ptr<ProtoDatabaseImpl<TestProto, T>> CreateDBImpl(
      ProtoDbType db_type,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<SharedProtoDatabaseProvider> db_provider) {
    return std::make_unique<ProtoDatabaseImpl<TestProto, T>>(
        db_type, db_dir, task_runner, std::move(db_provider));
  }

  void GetDbAndWait(ProtoDatabaseProvider* db_provider, ProtoDbType db_type) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    auto db = db_provider->GetDB<TestProto, T>(db_type, temp_dir.GetPath(),
                                               GetTestThreadTaskRunner());

    base::RunLoop run_init;

    // Initialize a database, it should succeed.
    db->Init(base::BindOnce(
        [](base::OnceClosure closure, Enums::InitStatus status) {
          std::move(closure).Run();
          EXPECT_TRUE(status == Enums::InitStatus::kOK);
        },
        run_init.QuitClosure()));

    run_init.Run();

    // Destroy the db and run all tasks to allow resources to be released before
    // |temp_dir| is cleaned up.
    db.reset();
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderNoSharedDB() {
    // Create a test provider with a test shared DB that will return invalid
    // operation when a client is requested, level_db returns invalid operation
    // when a database doesn't exist.
    return CreateProviderWithTestSharedDB(Enums::InitStatus::kInvalidOperation);
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderWithSharedDB() {
    return std::make_unique<TestProtoDatabaseProvider>(
        shared_db_temp_dir_.GetPath(), shared_db_);
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderWithTestSharedDB(
      Enums::InitStatus shared_client_init_status) {
    auto test_shared_db = base::WrapRefCounted(new TestSharedProtoDatabase(
        kDefaultClientName, shared_db_temp_dir_.GetPath(),
        shared_client_init_status));

    return std::make_unique<TestProtoDatabaseProvider>(
        shared_db_temp_dir_.GetPath(), test_shared_db);
  }

  std::unique_ptr<TestSharedProtoDatabaseProvider> CreateSharedProvider(
      TestProtoDatabaseProvider* db_provider) {
    return std::make_unique<TestSharedProtoDatabaseProvider>(
        base::SequencedTaskRunner::GetCurrentDefault(),
        db_provider->weak_factory_.GetWeakPtr());
  }

  // Uses ProtoDatabaseImpl's 3 parameter Init to bypass the check that gets
  // |use_shared_db|'s value.
  void InitDBImpl(ProtoDatabaseImpl<TestProto, T>* db_impl,
                  const std::string& client_name,
                  bool use_shared_db,
                  Callbacks::InitStatusCallback callback) {
    db_impl->InitInternal(client_name, CreateSimpleOptions(), use_shared_db,
                          std::move(callback));
  }

  void InitDBImplAndWait(ProtoDatabaseImpl<TestProto, T>* db_impl,
                         const std::string& client_name,
                         bool use_shared_db,
                         Enums::InitStatus expect_status) {
    base::RunLoop init_loop;
    InitDBImpl(
        db_impl, client_name, use_shared_db,
        base::BindOnce(
            [](base::OnceClosure closure, Enums::InitStatus expect_status,
               Enums::InitStatus status) {
              ASSERT_EQ(status, expect_status);
              std::move(closure).Run();
            },
            init_loop.QuitClosure(), expect_status));
    init_loop.Run();
  }

  std::unique_ptr<TestSharedProtoDatabaseClient> GetSharedClient() {
    return std::make_unique<TestSharedProtoDatabaseClient>(shared_db_);
  }

  void CallOnGetSharedDBClientAndWait(
      std::unique_ptr<UniqueProtoDatabase> unique_db,
      Enums::InitStatus unique_db_status,
      bool use_shared_db,
      std::unique_ptr<SharedProtoDatabaseClient> shared_db_client,
      Enums::InitStatus shared_db_status,
      Enums::InitStatus expect_status) {
    base::RunLoop init_loop;

    scoped_refptr<ProtoDatabaseSelector> selector(new ProtoDatabaseSelector(
        ProtoDbType::TEST_DATABASE1, GetTestThreadTaskRunner(), nullptr));

    GetTestThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ProtoDatabaseSelector::OnGetSharedDBClient, selector,
            std::move(unique_db), unique_db_status, use_shared_db,
            base::BindOnce(
                [](base::OnceClosure closure, Enums::InitStatus expect_status,
                   Enums::InitStatus status) {
                  ASSERT_EQ(status, expect_status);
                  std::move(closure).Run();
                },
                init_loop.QuitClosure(), expect_status),
            std::move(shared_db_client), shared_db_status));

    init_loop.Run();

    // If the process succeeded then check that the selector has a database set.
    if (expect_status == Enums::InitStatus::kOK) {
      ASSERT_NE(selector->db_, nullptr);
    }
  }

  // Just uses each entry's key to fill out the id/data fields in TestProto as
  // well.
  void AddDataToDBImpl(ProtoDatabaseImpl<TestProto, T>* db_impl,
                       std::vector<std::string>* entry_keys) {
    auto data_set = std::make_unique<std::vector<std::pair<std::string, T>>>();
    for (const auto& key : *entry_keys) {
      T data;
      CreateData(key, key, &data);
      data_set->emplace_back(key, std::move(data));
    }

    base::RunLoop data_loop;
    db_impl->UpdateEntries(std::move(data_set),
                           std::make_unique<std::vector<std::string>>(),
                           base::BindOnce(
                               [](base::OnceClosure closure, bool success) {
                                 ASSERT_TRUE(success);
                                 std::move(closure).Run();
                               },
                               data_loop.QuitClosure()));
    data_loop.Run();
  }

  void VerifyDataInDBImpl(ProtoDatabaseImpl<TestProto, T>* db_impl,
                          std::vector<std::string>* entry_keys) {
    base::RunLoop load_loop;
    db_impl->LoadKeysAndEntries(base::BindOnce(
        [](base::OnceClosure closure, std::vector<std::string>* entry_keys,
           bool success,
           std::unique_ptr<std::map<std::string, T>> keys_entries) {
          ASSERT_TRUE(success);
          ASSERT_EQ(entry_keys->size(), keys_entries->size());

          for (const auto& key : *entry_keys) {
            auto search = keys_entries->find(key);
            ASSERT_TRUE(search != keys_entries->end());
            // CreateData above uses double key as id and triple key as data.
            ASSERT_EQ(key + key, search->second.id());
            ASSERT_EQ(key + key + key, search->second.data());
          }
          std::move(closure).Run();
        },
        load_loop.QuitClosure(), entry_keys));
    load_loop.Run();
  }

  void UpdateClientMetadata(
      SharedDBMetadataProto::MigrationStatus migration_status) {
    base::RunLoop init_wait;
    auto client = shared_db_->GetClientForTesting(
        ProtoDbType::TEST_DATABASE1, /*create_if_missing=*/true,
        base::BindOnce(
            [](base::OnceClosure closure, Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(Enums::kOK, status);
              std::move(closure).Run();
            },
            init_wait.QuitClosure()));
    init_wait.Run();

    base::RunLoop wait_loop;
    shared_db_->UpdateClientMetadataAsync(
        client->client_db_id(), migration_status,
        base::BindOnce(
            [](base::OnceClosure closure, bool success) {
              EXPECT_TRUE(success);
              std::move(closure).Run();
            },
            wait_loop.QuitClosure()));
    wait_loop.Run();
  }

  SharedDBMetadataProto::MigrationStatus GetClientMigrationStatus() {
    SharedDBMetadataProto::MigrationStatus migration_status;
    base::RunLoop init_wait;
    auto client = shared_db_->GetClientForTesting(
        ProtoDbType::TEST_DATABASE1, /*create_if_missing=*/true,
        base::BindOnce(
            [](base::OnceClosure closure,
               SharedDBMetadataProto::MigrationStatus* output,
               Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(Enums::kOK, status);
              *output = migration_status;
              std::move(closure).Run();
            },
            init_wait.QuitClosure(), &migration_status));
    init_wait.Run();

    return migration_status;
  }

  scoped_refptr<base::SequencedTaskRunner> GetTestThreadTaskRunner() {
    return test_task_runner_;
  }

  base::FilePath temp_dir() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir shared_db_temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  // Shared database.
  scoped_refptr<base::SequencedTaskRunner> test_task_runner_;
  scoped_refptr<SharedProtoDatabase> shared_db_;
};

using ProtoDatabaseImplTestConfig = testing::Types<TestProto, ClientStruct>;
TYPED_TEST_SUITE(ProtoDatabaseImplTest, ProtoDatabaseImplTestConfig);

TYPED_TEST(ProtoDatabaseImplTest, FailsBothDatabases) {
  auto db_provider = this->CreateProviderNoSharedDB();
  auto shared_db_provider = this->CreateSharedProvider(db_provider.get());
  auto db_impl =
      this->CreateDBImpl(ProtoDbType::TEST_DATABASE1, this->temp_dir(),
                         this->GetTestThreadTaskRunner(),
                         this->CreateSharedProvider(db_provider.get()));
  this->InitDBImplAndWait(db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kError);
}

TYPED_TEST(ProtoDatabaseImplTest, Fails_UseShared_NoSharedDB) {
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(this->GetTestThreadTaskRunner());

  // If a shared DB is requested, and it fails to open for any reason then we
  // return a failure, the shared DB is opened using create_if_missing = true,
  // so we shouldn't get a missing DB.
  this->CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      true,                        // We should be using a shared DB.
      nullptr,                     // Shared DB failed to open.
      Enums::InitStatus::kError,   // Shared DB had an IO error.
      Enums::InitStatus::kError);  // Then the DB impl should return an error.

  this->CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      true,                                  // We should be using a shared DB.
      nullptr,                               // Shared DB failed to open.
      Enums::InitStatus::kInvalidOperation,  // Shared DB doesn't exist.
      Enums::InitStatus::kError);  // Then the DB impl should return an error.
}

TYPED_TEST(ProtoDatabaseImplTest,
           SucceedsWithShared_UseShared_HasSharedDB_UniqueNotFound) {
  auto shared_db_client = this->GetSharedClient();

  // Migration status is not attempted.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);

  // If we request a shared DB, the unique DB fails to open because it doesn't
  // exist and a migration hasn't been attempted then we return the shared DB
  // and we set the migration status to migrated to shared.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the DB impl should return the shared DB.
}

TYPED_TEST(ProtoDatabaseImplTest,
           Fails_UseShared_HasSharedDB_UniqueHadIOError) {
  auto shared_db_client = this->GetSharedClient();

  // Migration status is not attempted.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);

  // If we request a shared DB, the unique DB fails to open because of an IO
  // error and a migration hasn't been attempted then we throw an error, as the
  // unique DB could contain data yet to be migrated.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kError);  // Then the DB impl should return an error.
}

TYPED_TEST(ProtoDatabaseImplTest,
           SuccedsWithShared_UseShared_HasSharedDB_DataWasMigratedToShared) {
  auto shared_db_client = this->GetSharedClient();

  // Database has been migrated to Shared.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);

  // If we request a shared DB, the unique DB fails to open for any reason and
  // the data has been migrated to the shared DB then we can return the shared
  // DB safely.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the DB impl should use the shared DB.

  shared_db_client = this->GetSharedClient();

  // Data has been migrated to Shared, Unique DB still exists and should be
  // removed.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  // This second scenario occurs when the unique DB is marked to be deleted, but
  // it fails to open, we should also return the unique DB without throwing an
  // error.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the DB impl should use the shared DB.
}

TYPED_TEST(ProtoDatabaseImplTest,
           SucceedsWithShared_UseShared_HasSharedDB_MigratedUniqueWasDeleted) {
  auto shared_db_client = this->GetSharedClient();

  // Database has been migrated to Unique.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL);

  // If we request a shared DB, the unique DB fails does not exist and
  // the data has been migrated to the unique DB then we return shared db.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB missing.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Returns shared db.

  shared_db_client = this->GetSharedClient();

  // Data has been migrated to Unique, but data still exists in Shared DB that
  // should be removed.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  // This second scenario occurs when the Shared DB still contains data, we
  // should still clear shared and return it.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB missing.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Returns shared db
}

TYPED_TEST(ProtoDatabaseImplTest,
           Fails_UseShared_HasSharedDB_DataWasMigratedToUnique) {
  auto shared_db_client = this->GetSharedClient();

  // Database has been migrated to Unique.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL);

  // If we request a shared DB, the unique DB fails to open for any reason and
  // the data has been migrated to the unique DB then we throw an error, as the
  // unique database may contain data.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB failure.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kError);  // Then the DB impl should throw an error.

  shared_db_client = this->GetSharedClient();

  // Data has been migrated to Unique, but data still exists in Shared DB that
  // should be removed.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  // This second scenario occurs when the Shared DB still contains data, we
  // should still throw an error.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kError);  // Then the DB impl should throw an error.
}

TYPED_TEST(
    ProtoDatabaseImplTest,
    SucceedsWithShared_DontUseShared_HasSharedDB_DataWasMigratedToShared) {
  auto shared_db_client = this->GetSharedClient();

  // Database has been migrated to Shared.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);

  // If we request a unique DB, the unique DB fails to open for any reason and
  // the data has been migrated to the shared DB then we use the Shared DB.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      false,                                 // We should be using a unique DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the DB impl should use the shared DB.

  shared_db_client = this->GetSharedClient();

  // Data has been migrated to Shared, but the Unique DB still exists and needs
  // to be deleted.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  // This second scenario occurs when the unique database is marked to be
  // deleted, we should still use the shared DB.
  this->CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the DB impl should use the shared DB.
}

TYPED_TEST(ProtoDatabaseImplTest,
           SucceedsWithUnique_DontUseShared_SharedDBNotFound) {
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(this->GetTestThreadTaskRunner());

  // If the shared DB client fails to open because it doesn't exist then we can
  // return the unique DB safely.
  this->CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      false,                                 // We should be using a unique DB.
      nullptr,                               // Shared DB failed to open.
      Enums::InitStatus::kInvalidOperation,  // Shared DB doesn't exist.
      Enums::InitStatus::kOK);  // Then the DB impl should return the unique DB.
}

TYPED_TEST(ProtoDatabaseImplTest, Fails_DontUseShared_SharedDBFailed) {
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(this->GetTestThreadTaskRunner());

  // If the shared DB client fails to open because of an IO error then we
  // shouldn't return a database, as the shared DB could contain data not yet
  // migrated.
  this->CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      false,                       // We should be using a unique DB.
      nullptr,                     // Shared DB failed to open.
      Enums::InitStatus::kError,   // Shared DB had an IO error.
      Enums::InitStatus::kError);  // Then the DB impl should return an error.
}

TYPED_TEST(ProtoDatabaseImplTest, Fails_UseShared_NoSharedDB_NoUniqueDB) {
  auto db_provider = this->CreateProviderNoSharedDB();
  auto db_impl =
      this->CreateDBImpl(ProtoDbType::TEST_DATABASE1, this->temp_dir(),
                         this->GetTestThreadTaskRunner(),
                         this->CreateSharedProvider(db_provider.get()));
  this->InitDBImplAndWait(db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kError);
}

// Migration tests:
TYPED_TEST(ProtoDatabaseImplTest, Migration_EmptyDBs_UniqueToShared) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = this->CreateProviderNoSharedDB();
  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_noshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  // Kill the DB impl so it doesn't have a lock on the DB anymore.
  unique_db_impl.reset();
  // DB impl posts a task to destroy its database, so we wait for that task to
  // complete.
  base::RunLoop destroy_loop;
  this->GetTestThreadTaskRunner()->PostTask(FROM_HERE,
                                            destroy_loop.QuitClosure());
  destroy_loop.Run();

  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_EmptyDBs_SharedToUnique) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider = this->CreateProviderWithSharedDB();
  auto shared_db_impl =
      this->CreateDBImpl(ProtoDbType::TEST_DATABASE1, this->temp_dir(),
                         this->GetTestThreadTaskRunner(),
                         this->CreateSharedProvider(db_provider.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);

  // As the unique DB doesn't exist then the DB impl sets the migration status
  // to migrated to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());

  auto unique_db_impl =
      this->CreateDBImpl(ProtoDbType::TEST_DATABASE1, this->temp_dir(),
                         this->GetTestThreadTaskRunner(),
                         this->CreateSharedProvider(db_provider.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_UniqueToShared) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = this->CreateProviderNoSharedDB();
  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_noshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(unique_db_impl.get(), data_set.get());
  // Kill the DB impl so it doesn't have a lock on the DB anymore.
  unique_db_impl.reset();
  // DB impl posts a task to destroy its database, so we wait for that task to
  // complete.
  base::RunLoop destroy_loop;
  this->GetTestThreadTaskRunner()->PostTask(FROM_HERE,
                                            destroy_loop.QuitClosure());
  destroy_loop.Run();

  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);
  this->VerifyDataInDBImpl(shared_db_impl.get(), data_set.get());

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_SharedToUnique) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(shared_db_impl.get(), data_set.get());

  // As the unique DB doesn't exist then the DB impl sets the migration status
  // to migrated to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());

  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  this->VerifyDataInDBImpl(unique_db_impl.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_UniqueToShared_UniqueObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = this->CreateProviderNoSharedDB();
  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_noshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(unique_db_impl.get(), data_set.get());
  // Kill the DB impl so it doesn't have a lock on the DB anymore.
  unique_db_impl.reset();

  this->UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);

  // Unique DB should be deleted in migration. So, shared DB should be clean.
  data_set->clear();
  this->VerifyDataInDBImpl(shared_db_impl.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_UniqueToShared_SharedObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(shared_db_impl.get(), data_set.get());

  // As there's no unique DB, the DB impl is going to set the state to migrated
  // to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());

  // Force create an unique DB, which was deleted by migration.
  auto db_provider_noshared = this->CreateProviderNoSharedDB();
  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_noshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  unique_db_impl.reset();

  this->UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  shared_db_impl.reset();
  db_provider_withshared = this->CreateProviderWithSharedDB();

  auto shared_db_impl1 = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl1.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);

  // Shared DB should be deleted in migration. So, shared DB should be clean.
  data_set->clear();
  this->VerifyDataInDBImpl(shared_db_impl1.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_SharedToUnique_SharedObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(shared_db_impl.get(), data_set.get());

  // As there's no Unique DB, the DB impl changes the migration status to
  // migrated to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());

  this->UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);

  // Shared DB should be deleted in migration. So, unique DB should be clean.
  data_set->clear();
  this->VerifyDataInDBImpl(unique_db_impl.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, Migration_SharedToUnique_UniqueObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_noshared = this->CreateProviderNoSharedDB();
  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_noshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(unique_db_impl.get(), data_set.get());

  this->UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  unique_db_impl.reset();

  auto db_provider_withshared = this->CreateProviderWithSharedDB();
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);

  // Unique DB should be deleted in migration. So, unique DB should be clean.
  data_set->clear();
  this->VerifyDataInDBImpl(shared_db_impl.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, InMemoryDatabaseDoesNoMigration) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");
  auto db_provider_withshared = this->CreateProviderWithSharedDB();

  // First we create a shared DB so our second pass has a shared DB available.
  auto shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);
  this->AddDataToDBImpl(shared_db_impl.get(), data_set.get());

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());
  shared_db_impl.reset();

  // Open in memory database (unique db). This should not migrate the data from
  // shared db.
  auto unique_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, base::FilePath(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(unique_db_impl.get(), kDefaultClientName, false,
                          Enums::InitStatus::kOK);

  auto empty_data = std::make_unique<std::vector<std::string>>();
  this->VerifyDataInDBImpl(unique_db_impl.get(), empty_data.get());
  unique_db_impl.reset();

  // Open shared db again to check the old data is present.
  shared_db_impl = this->CreateDBImpl(
      ProtoDbType::TEST_DATABASE1, this->temp_dir(),
      this->GetTestThreadTaskRunner(),
      this->CreateSharedProvider(db_provider_withshared.get()));
  this->InitDBImplAndWait(shared_db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kOK);
  this->VerifyDataInDBImpl(shared_db_impl.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            this->GetClientMigrationStatus());
}

TYPED_TEST(ProtoDatabaseImplTest, DestroyShouldWorkWhenUniqueInitFailed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto db_provider = this->CreateProviderNoSharedDB();
  auto shared_db_provider = this->CreateSharedProvider(db_provider.get());
  auto db_impl =
      this->CreateDBImpl(ProtoDbType::TEST_DATABASE1, temp_dir.GetPath(),
                         this->GetTestThreadTaskRunner(),
                         this->CreateSharedProvider(db_provider.get()));

  // Try to initialize a db and fail.
  this->InitDBImplAndWait(db_impl.get(), kDefaultClientName, true,
                          Enums::InitStatus::kError);

  base::RunLoop run_destroy;

  // Call destroy on the db, it should destroy the db directory.
  db_impl->Destroy(base::BindOnce(
      [](base::OnceClosure closure, bool success) {
        std::move(closure).Run();
        EXPECT_TRUE(success);
      },
      run_destroy.QuitClosure()));

  run_destroy.Run();

  // Verify the db is actually destroyed.
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath()));
}

TYPED_TEST(ProtoDatabaseImplTest, InitWithOptions) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir.GetPath()));

  auto db_provider = this->CreateProviderNoSharedDB();
  auto db_impl =
      this->CreateDBImpl(ProtoDbType::TEST_DATABASE1, temp_dir.GetPath(),
                         this->GetTestThreadTaskRunner(),
                         this->CreateSharedProvider(db_provider.get()));

  base::RunLoop run_init;
  auto options = CreateSimpleOptions();
  options.create_if_missing = false;

  // Initialize database with unique DB arguments, it should fail because we
  // specified create_if_missing = false and there's no shared DB.
  db_impl->Init(
      options,
      base::BindOnce(
          [](base::OnceClosure closure, Enums::InitStatus expect_status,
             Enums::InitStatus status) {
            ASSERT_EQ(status, expect_status);
            std::move(closure).Run();
          },
          run_init.QuitClosure(), Enums::InitStatus::kError));

  run_init.Run();
}

TYPED_TEST(ProtoDatabaseImplTest, InitUniqueTwiceShouldSucceed) {
  base::ScopedTempDir temp_dir_profile;
  ASSERT_TRUE(temp_dir_profile.CreateUniqueTempDir());

  // Both databases will be opened as unique.
  auto experiment_params = std::map<std::string, std::string>{
      {"migrate_TestDatabase1", "false"}, {"migrate_TestDatabase2", "false"}};
  this->SetUpExperimentParams(experiment_params);

  auto db_provider =
      std::make_unique<ProtoDatabaseProvider>(temp_dir_profile.GetPath());

  // Initialize a database, it should succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE1);
  // Initialize a second database, it should also succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE2);

  // Destroy the db provider and run all tasks to allow resources to be released
  // before |temp_dir| is cleaned up.
  db_provider.reset();
  this->task_environment_.RunUntilIdle();
}

TYPED_TEST(ProtoDatabaseImplTest, InitUniqueThenSharedShouldSucceed) {
  base::ScopedTempDir temp_dir_profile;
  ASSERT_TRUE(temp_dir_profile.CreateUniqueTempDir());

  // First database will open as unique, second DB will open as shared.
  auto experiment_params = std::map<std::string, std::string>{
      {"migrate_TestDatabase1", "false"}, {"migrate_TestDatabase2", "true"}};
  this->SetUpExperimentParams(experiment_params);

  auto db_provider =
      std::make_unique<ProtoDatabaseProvider>(temp_dir_profile.GetPath());

  // Initialize a database, it should succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE1);
  // Initialize a second database, it should also succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE2);

  // Destroy the db provider and run all tasks to allow resources to be released
  // before |temp_dir_profile| is cleaned up.
  db_provider.reset();
  this->task_environment_.RunUntilIdle();
}

TYPED_TEST(ProtoDatabaseImplTest, InitSharedThenUniqueShouldSucceed) {
  base::ScopedTempDir temp_dir_profile;
  ASSERT_TRUE(temp_dir_profile.CreateUniqueTempDir());

  // First database will open as shared, second DB will open as unique.
  auto experiment_params = std::map<std::string, std::string>{
      {"migrate_TestDatabase1", "true"}, {"migrate_TestDatabase2", "false"}};
  this->SetUpExperimentParams(experiment_params);

  auto db_provider =
      std::make_unique<ProtoDatabaseProvider>(temp_dir_profile.GetPath());

  // Initialize a database, it should succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE1);
  // Initialize a second database, it should also succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE2);

  // Destroy the db provider and run all tasks to allow resources to be released
  // before |temp_dir_profile| is cleaned up.
  db_provider.reset();
  this->task_environment_.RunUntilIdle();
}

TYPED_TEST(ProtoDatabaseImplTest, InitSharedTwiceShouldSucceed) {
  base::ScopedTempDir temp_dir_profile;
  ASSERT_TRUE(temp_dir_profile.CreateUniqueTempDir());

  // Both databases will open as shared.
  auto experiment_params = std::map<std::string, std::string>{
      {"migrate_TestDatabase1", "true"}, {"migrate_TestDatabase2", "true"}};
  this->SetUpExperimentParams(experiment_params);

  auto db_provider =
      std::make_unique<ProtoDatabaseProvider>(temp_dir_profile.GetPath());

  // Initialize a database, it should succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE1);
  // Initialize a second database, it should also succeed.
  this->GetDbAndWait(db_provider.get(), ProtoDbType::TEST_DATABASE2);

  // Destroy the db provider and run all tasks to allow resources to be released
  // before |temp_dir_profile| is cleaned up.
  db_provider.reset();
  this->task_environment_.RunUntilIdle();
}

}  // namespace leveldb_proto
