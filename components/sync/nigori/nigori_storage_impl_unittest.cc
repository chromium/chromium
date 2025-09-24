// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_storage_impl.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/test_encryptor.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

os_crypt_async::Encryptor GetInstanceSync(
    os_crypt_async::OSCryptAsync* factory) {
  base::test::TestFuture<os_crypt_async::Encryptor> future;
  factory->GetInstance(future.GetCallback(),
                       os_crypt_async::Encryptor::Option::kNone);
  return future.Take();
}

sync_pb::NigoriLocalData MakeSomeNigoriLocalData() {
  sync_pb::NigoriLocalData result;
  result.mutable_data_type_state()->set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  result.mutable_entity_metadata()->set_sequence_number(1);
  result.mutable_nigori_model()->set_encrypt_everything(true);
  return result;
}

class NigoriStorageImplTest : public testing::Test {
 protected:
  NigoriStorageImplTest() = default;
  ~NigoriStorageImplTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetFilePath() {
    return temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("some_file")));
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(NigoriStorageImplTest, ShouldBeAbleToRestoreAfterWriteWithAsync) {
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);

  NigoriStorageImpl writer_storage(GetFilePath(),
                                   std::make_unique<os_crypt_async::Encryptor>(
                                       GetInstanceSync(os_crypt.get())));
  sync_pb::NigoriLocalData write_data = MakeSomeNigoriLocalData();
  writer_storage.StoreData(write_data);

  // Use different NigoriStorageImpl when reading to avoid dependency on its
  // state and emulate browser restart.
  NigoriStorageImpl reader_storage(GetFilePath(),
                                   std::make_unique<os_crypt_async::Encryptor>(
                                       GetInstanceSync(os_crypt.get())));
  std::optional<sync_pb::NigoriLocalData> read_data =
      reader_storage.RestoreData();
  EXPECT_NE(read_data, std::nullopt);
  EXPECT_EQ(read_data->SerializeAsString(), write_data.SerializeAsString());
}

TEST_F(NigoriStorageImplTest, ShouldBeAbleToRestoreAfterWriteWithSync) {
  OSCryptMocker::SetUp();
  NigoriStorageImpl writer_storage(GetFilePath(), /*encryptor=*/nullptr);
  sync_pb::NigoriLocalData write_data = MakeSomeNigoriLocalData();
  writer_storage.StoreData(write_data);

  // Use different NigoriStorageImpl when reading to avoid dependency on its
  // state and emulate browser restart.
  NigoriStorageImpl reader_storage(GetFilePath(), /*encryptor=*/nullptr);
  std::optional<sync_pb::NigoriLocalData> read_data =
      reader_storage.RestoreData();

  EXPECT_NE(read_data, std::nullopt);
  EXPECT_EQ(read_data->SerializeAsString(), write_data.SerializeAsString());

  OSCryptMocker::TearDown();
}

TEST_F(NigoriStorageImplTest, ShouldReturnNulloptWhenFileNotExists) {
  NigoriStorageImpl storage(GetFilePath(), /*encryptor=*/nullptr);
  EXPECT_EQ(storage.RestoreData(), std::nullopt);
}

TEST_F(NigoriStorageImplTest, ShouldRemoveFile) {
  OSCryptMocker::SetUp();
  NigoriStorageImpl storage(GetFilePath(), /*encryptor=*/nullptr);
  sync_pb::NigoriLocalData data = MakeSomeNigoriLocalData();
  storage.StoreData(data);
  ASSERT_TRUE(base::PathExists(GetFilePath()));
  storage.ClearData();
  EXPECT_FALSE(base::PathExists(GetFilePath()));
  OSCryptMocker::TearDown();
}

}  // namespace

}  // namespace syncer
