// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_storage.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/hash.h"
#include "crypto/obsolete/md5.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::Ge;

MATCHER_P(KeyMaterialEq, expected, "") {
  const std::string& key_material = arg.key_material();
  const std::vector<uint8_t> key_material_as_bytes(key_material.begin(),
                                                   key_material.end());
  return key_material_as_bytes == expected;
}

base::FilePath CreateUniqueTempDir(base::ScopedTempDir* temp_dir) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

bool WriteLocalTrustedVaultFile(
    const trusted_vault_pb::LocalTrustedVault& proto,
    const base::FilePath& path) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(proto.SerializeAsString());
  file_proto.set_md5_digest_hex_string(
      MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()));
  if (base::FeatureList::IsEnabled(kEnableTrustedVaultSHA256)) {
    file_proto.set_sha256_digest_hex_string(
        base::Base64Encode(crypto::hash::Sha256(
            base::as_byte_span(file_proto.serialized_local_trusted_vault()))));
  }
  return base::WriteFile(path, file_proto.SerializeAsString());
}

trusted_vault_pb::LocalTrustedVault ReadLocalTrustedVaultFile(
    const base::FilePath& path) {
  std::string file_content;
  trusted_vault_pb::LocalTrustedVault data_proto;
  if (!base::ReadFileToString(path, &file_content)) {
    return data_proto;
  }
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  if (!file_proto.ParseFromString(file_content)) {
    return data_proto;
  }

  if (MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()) !=
      file_proto.md5_digest_hex_string()) {
    return data_proto;
  }

  if (base::FeatureList::IsEnabled(kEnableTrustedVaultSHA256)) {
    if ((base::Base64Encode(crypto::hash::Sha256(base::as_byte_span(
            file_proto.serialized_local_trusted_vault())))) !=
        file_proto.sha256_digest_hex_string()) {
      return data_proto;
    }
  }
  data_proto.ParseFromString(file_proto.serialized_local_trusted_vault());
  return data_proto;
}

class StandaloneTrustedVaultStorageTest : public testing::Test {
 public:
  StandaloneTrustedVaultStorageTest()
      : base_path_(CreateUniqueTempDir(&temp_dir_)),
        storage_(std::make_unique<StandaloneTrustedVaultStorage>(
            base_path(),
            security_domain_id())) {}

  ~StandaloneTrustedVaultStorageTest() override = default;

  SecurityDomainId security_domain_id() const {
    return SecurityDomainId::kChromeSync;
  }

  std::string security_domain_name_for_uma() const {
    return GetSecurityDomainNameForUma(security_domain_id());
  }

  const base::FilePath& base_path() { return base_path_; }

  const base::FilePath file_path() {
    // |security_domain_id| is |kChromeSync|, thus the file name is
    // "trusted_vault.pb".
    return base_path_.Append(
        base::FilePath(FILE_PATH_LITERAL("trusted_vault.pb")));
  }

  StandaloneTrustedVaultStorage* storage() { return storage_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  const base::FilePath base_path_;
  std::unique_ptr<StandaloneTrustedVaultStorage> storage_;
};

TEST_F(StandaloneTrustedVaultStorageTest, ShouldRecordNotFoundWhenReadingFile) {
  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kNotFound,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultStorageTest,
       ShouldRecordMD5DigestMismatchWhenReadingFile) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_md5_digest_hex_string("corrupted_md5_digest");
  ASSERT_TRUE(base::WriteFile(file_path(), file_proto.SerializeAsString()));

  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kMD5DigestMismatch,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultStorageTest,
       ShouldRecordFileProtoDeserializationFailedWhenReadingFile) {
  ASSERT_TRUE(base::WriteFile(file_path(), "corrupted_proto"));

  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultFileReadStatusForUMA::kFileProtoDeserializationFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultStorageTest,
       ShouldRecordDataProtoDeserializationFailedWhenReadingFile) {
  const std::string kCorruptedSerializedDataProto = "corrupted_proto";
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(kCorruptedSerializedDataProto);
  file_proto.set_md5_digest_hex_string(
      MD5StringForTrustedVault(kCorruptedSerializedDataProto));
  ASSERT_TRUE(base::WriteFile(file_path(), file_proto.SerializeAsString()));

  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/
      TrustedVaultFileReadStatusForUMA::kDataProtoDeserializationFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultStorageTest, ShouldReadAndFindUserKeys) {
  const GaiaId kGaiaId1("user1");
  const GaiaId kGaiaId2("user2");
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};
  const std::vector<uint8_t> kKey4 = {3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(kGaiaId1.ToString());
  user_data2->set_gaia_id(kGaiaId2.ToString());
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data1->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());
  user_data2->add_vault_key()->set_key_material(kKey4.data(), kKey4.size());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));
  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);

  trusted_vault_pb::LocalTrustedVaultPerUser* stored_user_data1 =
      storage()->FindUserVault(kGaiaId1);
  EXPECT_THAT(stored_user_data1->vault_key(),
              ElementsAre(KeyMaterialEq(kKey1), KeyMaterialEq(kKey2)));
  trusted_vault_pb::LocalTrustedVaultPerUser* stored_user_data2 =
      storage()->FindUserVault(kGaiaId2);
  EXPECT_THAT(stored_user_data2->vault_key(),
              ElementsAre(KeyMaterialEq(kKey3), KeyMaterialEq(kKey4)));
}

TEST_F(StandaloneTrustedVaultStorageTest, ShouldAddAndStoreUserKeys) {
  const GaiaId kGaiaId1("user1");
  const GaiaId kGaiaId2("user2");
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  base::HistogramTester histogram_tester;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      storage()->AddUserVault(kGaiaId1);
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data1->set_last_vault_key_version(7);
  storage()->WriteDataToDisk();

  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      storage()->AddUserVault(kGaiaId2);
  user_data2->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());
  user_data2->set_last_vault_key_version(9);
  storage()->WriteDataToDisk();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileWriteSuccess." + security_domain_name_for_uma(),
      /*sample=*/true,
      /*expected_bucket_count=*/2);

  // Read the file from disk.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(2));
  EXPECT_THAT(proto.user(0).vault_key(), ElementsAre(KeyMaterialEq(kKey1)));
  EXPECT_THAT(proto.user(0).last_vault_key_version(), Eq(7));
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(kKey2), KeyMaterialEq(kKey3)));
  EXPECT_THAT(proto.user(1).last_vault_key_version(), Eq(9));
}

TEST_F(StandaloneTrustedVaultStorageTest, ShouldRemoveUser) {
  const GaiaId kGaiaId1("user1");
  const GaiaId kGaiaId2("user2");
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};
  const std::vector<uint8_t> kKey4 = {3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(kGaiaId1.ToString());
  user_data2->set_gaia_id(kGaiaId2.ToString());
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data1->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());
  user_data2->add_vault_key()->set_key_material(kKey4.data(), kKey4.size());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));
  storage()->ReadDataFromDisk();

  storage()->RemoveUserVaults(
      [&](const trusted_vault_pb::LocalTrustedVaultPerUser& user_data) -> bool {
        return user_data.gaia_id() == kGaiaId2.ToString();
      });

  storage()->WriteDataToDisk();

  // Read the file from disk.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(1));
  EXPECT_THAT(proto.user(0).gaia_id(), Eq(kGaiaId1.ToString()));
  EXPECT_THAT(proto.user(0).vault_key(),
              ElementsAre(KeyMaterialEq(kKey1), KeyMaterialEq(kKey2)));
}

TEST_F(StandaloneTrustedVaultStorageTest,
       ShouldUpgradeToVersion1AndFixMissingConstantKey) {
  const char gaia_id_1[] = "user1";
  const char gaia_id_2[] = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(gaia_id_1);
  user_data2->set_gaia_id(gaia_id_2);
  // Mimic |user_data1| to be affected by crbug.com/1267391 and |user_data2| to
  // be not affected.
  AssignBytesToProtoString(kKey1,
                           user_data1->add_vault_key()->mutable_key_material());
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           user_data2->add_vault_key()->mutable_key_material());
  AssignBytesToProtoString(kKey2,
                           user_data2->add_vault_key()->mutable_key_material());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));
  // Storage should fix corrupted data and write new state.
  storage()->ReadDataFromDisk();

  // Read the file from disk.
  trusted_vault_pb::LocalTrustedVault proto =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(proto.user_size(), Eq(2));
  // Constant key should be added for the first user.
  EXPECT_THAT(proto.user(0).vault_key(),
              ElementsAre(KeyMaterialEq(GetConstantTrustedVaultKey()),
                          KeyMaterialEq(kKey1)));
  // Sanity check that state for the second user isn't changed.
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(GetConstantTrustedVaultKey()),
                          KeyMaterialEq(kKey2)));
  EXPECT_THAT(proto.data_version(), Ge(1));
}

TEST_F(StandaloneTrustedVaultStorageTest,
       ShouldUpgradeAllUsersDataToVersion2AndResetKeysAreStale) {
  const char gaia_id_1[] = "user1";
  const char gaia_id_2[] = "user2";

  trusted_vault_pb::LocalTrustedVault initial_data;
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data1->set_gaia_id(gaia_id_1);
  user_data1->set_keys_marked_as_stale_by_consumer(true);
  user_data2->set_gaia_id(gaia_id_2);
  user_data2->set_keys_marked_as_stale_by_consumer(true);
  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));

  // Storage should reset |keys_marked_as_stale_by_consumer| for both accounts
  // and write new state.
  storage()->ReadDataFromDisk();

  trusted_vault_pb::LocalTrustedVault new_data =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(new_data.user_size(), Eq(2));
  EXPECT_FALSE(new_data.user(0).keys_marked_as_stale_by_consumer());
  EXPECT_FALSE(new_data.user(1).keys_marked_as_stale_by_consumer());
  EXPECT_THAT(new_data.data_version(), Ge(2));
}

TEST_F(StandaloneTrustedVaultStorageTest, ShouldUpgradeToVersion3) {
  const char gaia_id_1[] = "user1";
  const char gaia_id_2[] = "user2";
  const auto key_pair = SecureBoxKeyPair::GenerateRandom();

  trusted_vault_pb::LocalTrustedVault initial_data;

  // First user has `device_registered_version` set to 0.
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  user_data1->set_gaia_id(gaia_id_1);
  user_data1->mutable_local_device_registration_info()->set_device_registered(
      true);
  user_data1->mutable_local_device_registration_info()
      ->set_device_registered_version(0);
  AssignBytesToProtoString(key_pair->private_key().ExportToBytes(),
                           user_data1->mutable_local_device_registration_info()
                               ->mutable_private_key_material());

  // Second user has `device_registered_version` set to 1.
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data2->set_gaia_id(gaia_id_2);
  user_data2->mutable_local_device_registration_info()->set_device_registered(
      true);
  user_data2->mutable_local_device_registration_info()
      ->set_device_registered_version(1);
  user_data2->set_keys_marked_as_stale_by_consumer(true);
  AssignBytesToProtoString(key_pair->private_key().ExportToBytes(),
                           user_data2->mutable_local_device_registration_info()
                               ->mutable_private_key_material());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));

  // Storage should reset `device_registered` for the first user (since
  // `device_registered_version` is 0), but keep it for the second user (since
  // `device_registered_version` is 1).
  storage()->ReadDataFromDisk();

  trusted_vault_pb::LocalTrustedVault new_data =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(new_data.user_size(), Eq(2));
  EXPECT_FALSE(
      new_data.user(0).local_device_registration_info().device_registered());
  EXPECT_TRUE(
      new_data.user(1).local_device_registration_info().device_registered());
  EXPECT_THAT(new_data.data_version(), Ge(3));
}

TEST_F(StandaloneTrustedVaultStorageTest, ShouldUpgradeToVersion4) {
  const char gaia_id_1[] = "user1";
  const char gaia_id_2[] = "user2";
  const auto key_pair = SecureBoxKeyPair::GenerateRandom();

  trusted_vault_pb::LocalTrustedVault initial_data;

  // First user has `local_device_registration_info.
  // last_registration_returned_local_data_obsolete` unset.
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data1 =
      initial_data.add_user();
  user_data1->set_gaia_id(gaia_id_1);
  user_data1->mutable_local_device_registration_info()->set_device_registered(
      true);
  AssignBytesToProtoString(key_pair->private_key().ExportToBytes(),
                           user_data1->mutable_local_device_registration_info()
                               ->mutable_private_key_material());

  // Second user has `local_device_registration_info.
  // last_registration_returned_local_data_obsolete` set to true.
  trusted_vault_pb::LocalTrustedVaultPerUser* user_data2 =
      initial_data.add_user();
  user_data2->set_gaia_id(gaia_id_2);
  user_data2->mutable_local_device_registration_info()->set_device_registered(
      true);
  user_data2->mutable_local_device_registration_info()
      ->set_deprecated_last_registration_returned_local_data_obsolete(true);
  AssignBytesToProtoString(key_pair->private_key().ExportToBytes(),
                           user_data2->mutable_local_device_registration_info()
                               ->mutable_private_key_material());

  ASSERT_TRUE(WriteLocalTrustedVaultFile(initial_data, file_path()));

  // Storage should populate per-user
  // `last_registration_returned_local_data_obsolete` from the deprecated
  // fields.
  storage()->ReadDataFromDisk();

  trusted_vault_pb::LocalTrustedVault new_data =
      ReadLocalTrustedVaultFile(file_path());
  ASSERT_THAT(new_data.user_size(), Eq(2));
  EXPECT_FALSE(
      new_data.user(0).has_last_registration_returned_local_data_obsolete());
  EXPECT_TRUE(
      new_data.user(1).last_registration_returned_local_data_obsolete());
  EXPECT_THAT(new_data.data_version(), Ge(4));
}

// This test ensures that migration logic in ReadDataFromDisk() doesn't create
// new file if there wasn't any.
TEST_F(StandaloneTrustedVaultStorageTest, ShouldNotWriteEmptyData) {
  storage()->ReadDataFromDisk();
  EXPECT_FALSE(base::PathExists(file_path()));
}

// This test checks that a corrupted SHA256 value is detected and returns the
// correct error code.
TEST_F(StandaloneTrustedVaultStorageTest,
       ShouldRecordSHA256DigestMismatchWhenReadingFile) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  // Set MD5 hash as normal since it takes precedence.
  file_proto.set_md5_digest_hex_string(
      MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()));
  file_proto.set_sha256_digest_hex_string("not_correct_digest");
  ASSERT_TRUE(base::WriteFile(file_path(), file_proto.SerializeAsString()));

  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kSHA256DigestMismatch,
      /*expected_bucket_count=*/1);
}

// This test checks that the `kEnableTrustedVaultSHA256` flag disables new
// SHA256 writes.
class StandaloneTrustedVaultStorageDisableSHA256Test
    : public StandaloneTrustedVaultStorageTest {
 public:
  StandaloneTrustedVaultStorageDisableSHA256Test() {
    feature_list_.InitAndDisableFeature(kEnableTrustedVaultSHA256);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that files written with corrupted SHA256 can still be read when the
// feature is disabled.
TEST_F(StandaloneTrustedVaultStorageDisableSHA256Test, DisablingSHA256) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  // Set MD5 hash as normal since it takes precedence.
  file_proto.set_md5_digest_hex_string(
      MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()));
  // Set corrupted sha256
  file_proto.set_sha256_digest_hex_string("not_correct_digest");
  ASSERT_TRUE(base::WriteFile(file_path(), file_proto.SerializeAsString()));

  // Read the file from disk.
  base::HistogramTester histogram_tester;
  storage()->ReadDataFromDisk();
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus." + security_domain_name_for_uma(),
      /*sample=*/TrustedVaultFileReadStatusForUMA::kSuccess,
      /*expected_bucket_count=*/1);
}

}  // namespace

}  // namespace trusted_vault
