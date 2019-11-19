// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_storage_impl.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/sync/base/fake_encryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

sync_pb::NigoriLocalData MakeSomeNigoriLocalData() {
  sync_pb::NigoriLocalData result;
  result.mutable_model_type_state()->set_initial_sync_done(true);
  result.mutable_entity_metadata()->set_sequence_number(1);
  result.mutable_nigori_model()->set_encrypt_everything(true);
  return result;
}

class NigoriStorageImplTest : public testing::Test {
 protected:
  NigoriStorageImplTest() = default;
  ~NigoriStorageImplTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const Encryptor* encryptor() { return &encryptor_; }

  base::FilePath GetFilePath() {
    return temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("some_file")));
  }

 private:
  FakeEncryptor encryptor_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(NigoriStorageImplTest, ShouldBeAbleToRestoreAfterWrite) {
  NigoriStorageImpl writer_storage(GetFilePath(), encryptor());
  sync_pb::NigoriLocalData write_data = MakeSomeNigoriLocalData();
  writer_storage.StoreData(write_data);

  // Use different NigoriStorageImpl when reading to avoid dependency on its
  // state and emulate browser restart.
  NigoriStorageImpl reader_storage(GetFilePath(), encryptor());
  base::Optional<sync_pb::NigoriLocalData> read_data =
      reader_storage.RestoreData();
  EXPECT_NE(read_data, base::nullopt);
  EXPECT_EQ(read_data->SerializeAsString(), write_data.SerializeAsString());
}

TEST_F(NigoriStorageImplTest, ShouldReturnNulloptWhenFileNotExists) {
  NigoriStorageImpl storage(GetFilePath(), encryptor());
  EXPECT_EQ(storage.RestoreData(), base::nullopt);
}

TEST_F(NigoriStorageImplTest, ShouldRemoveFile) {
  NigoriStorageImpl storage(GetFilePath(), encryptor());
  sync_pb::NigoriLocalData data = MakeSomeNigoriLocalData();
  storage.StoreData(data);
  ASSERT_TRUE(base::PathExists(GetFilePath()));
  storage.ClearData();
  EXPECT_FALSE(base::PathExists(GetFilePath()));
}

}  // namespace

}  // namespace syncer
