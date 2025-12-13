// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_storage.h"

#include <memory>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/hash.h"
#include "crypto/obsolete/md5.h"

namespace trusted_vault {

// This is a separate function and not in `namespace {}` so it can be friended
// by crypto/obsolete/md5, as required for using that class.
std::string MD5StringForTrustedVault(const std::string& local_trusted_value) {
  return base::HexEncodeLower(crypto::obsolete::Md5::Hash(local_trusted_value));
}

namespace {

constexpr base::FilePath::CharType kChromeSyncTrustedVaultFilename[] =
    FILE_PATH_LITERAL("trusted_vault.pb");
constexpr base::FilePath::CharType kPasskeysTrustedVaultFilename[] =
    FILE_PATH_LITERAL("passkeys_trusted_vault.pb");

constexpr int kCurrentLocalTrustedVaultVersion = 4;

base::FilePath GetBackendFilePath(const base::FilePath& base_dir,
                                  SecurityDomainId security_domain) {
  switch (security_domain) {
    case SecurityDomainId::kChromeSync:
      return base_dir.Append(kChromeSyncTrustedVaultFilename);
    case SecurityDomainId::kPasskeys:
      return base_dir.Append(kPasskeysTrustedVaultFilename);
  }
  NOTREACHED();
}

trusted_vault_pb::LocalTrustedVault ReadDataFromDiskImpl(
    const base::FilePath& file_path,
    SecurityDomainId security_domain_id) {
  std::string file_content;

  trusted_vault_pb::LocalTrustedVault data_proto;
  if (!base::PathExists(file_path)) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id, TrustedVaultFileReadStatusForUMA::kNotFound);
    return data_proto;
  }
  if (!base::ReadFileToString(file_path, &file_content)) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id, TrustedVaultFileReadStatusForUMA::kFileReadFailed);
    return data_proto;
  }
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  if (!file_proto.ParseFromString(file_content)) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kFileProtoDeserializationFailed);
    return data_proto;
  }

  if (MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()) !=
      file_proto.md5_digest_hex_string()) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kMD5DigestMismatch);
    return data_proto;
  }

  if (base::FeatureList::IsEnabled(kEnableTrustedVaultSHA256)) {
    if (file_proto.has_sha256_digest_hex_string() &&
        (base::Base64Encode(crypto::hash::Sha256(base::as_byte_span(
             file_proto.serialized_local_trusted_vault()))) !=
         file_proto.sha256_digest_hex_string())) {
      RecordTrustedVaultFileReadStatus(
          security_domain_id,
          TrustedVaultFileReadStatusForUMA::kSHA256DigestMismatch);
      return data_proto;
    }
  }

  if (!data_proto.ParseFromString(
          file_proto.serialized_local_trusted_vault())) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kDataProtoDeserializationFailed);
    return data_proto;
  }
  RecordTrustedVaultFileReadStatus(security_domain_id,
                                   TrustedVaultFileReadStatusForUMA::kSuccess);
  return data_proto;
}

// Version 0 may contain corrupted data: missing constant key if the client
// was affected by crbug.com/1267391, this function injects constant key if it's
// not stored and there is exactly one non-constant key. |local_trusted_vault|
// must not be null and must have |version| set to 0.
void UpgradeToVersion1(
    trusted_vault_pb::LocalTrustedVault* local_trusted_vault) {
  CHECK(local_trusted_vault);
  CHECK_EQ(local_trusted_vault->data_version(), 0);

  std::string constant_key_as_proto_string;
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           &constant_key_as_proto_string);

  for (trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault :
       *local_trusted_vault->mutable_user()) {
    if (per_user_vault.vault_key_size() == 1 &&
        per_user_vault.vault_key(0).key_material() !=
            constant_key_as_proto_string) {
      // Add constant key in the beginning.
      *per_user_vault.add_vault_key() = per_user_vault.vault_key(0);
      per_user_vault.mutable_vault_key(0)->set_key_material(
          constant_key_as_proto_string);
    }
  }
  local_trusted_vault->set_data_version(1);
}

// Version 1 may contain `keys_marked_as_stale_by_consumer` (before the field
// was renamed) accidentally set to true, upgrade to version 2 resets it to
// false.
void UpgradeToVersion2(
    trusted_vault_pb::LocalTrustedVault* local_trusted_vault) {
  CHECK(local_trusted_vault);
  CHECK_EQ(local_trusted_vault->data_version(), 1);

  for (trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault :
       *local_trusted_vault->mutable_user()) {
    per_user_vault.set_keys_marked_as_stale_by_consumer(false);
  }
  local_trusted_vault->set_data_version(2);
}

// Version 2 may contain `device_registered_version` set to 0 or 1, this concept
// was introduced a while ago to address a bug, upgrade to version 3 resets
// device registered flag to false if `device_registered_version` is 0, so the
// rest of the implementation doesn't need to handle this case.
void UpgradeToVersion3(
    trusted_vault_pb::LocalTrustedVault* local_trusted_vault) {
  CHECK(local_trusted_vault);
  CHECK_EQ(local_trusted_vault->data_version(), 2);

  for (trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault :
       *local_trusted_vault->mutable_user()) {
    if (per_user_vault.local_device_registration_info()
            .device_registered_version() == 0) {
      per_user_vault.mutable_local_device_registration_info()
          ->set_device_registered(false);
    }
  }
  local_trusted_vault->set_data_version(3);
}

// Version 3 had the `last_registration_returned_local_data_obsolete` field in
// `LocalDeviceRegistrationInfo` message. That was migrated to the
// `LocalTrustedVaultPerUser` message in version 4.
void UpgradeToVersion4(
    trusted_vault_pb::LocalTrustedVault* local_trusted_vault) {
  CHECK(local_trusted_vault);
  CHECK_EQ(local_trusted_vault->data_version(), 3);

  for (trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault :
       *local_trusted_vault->mutable_user()) {
    if (per_user_vault.local_device_registration_info()
            .has_deprecated_last_registration_returned_local_data_obsolete()) {
      per_user_vault.set_last_registration_returned_local_data_obsolete(
          per_user_vault.local_device_registration_info()
              .deprecated_last_registration_returned_local_data_obsolete());
    }
  }
  local_trusted_vault->set_data_version(4);
}

void WriteDataToDiskImpl(const trusted_vault_pb::LocalTrustedVault& data,
                         const base::FilePath& file_path,
                         SecurityDomainId security_domain_id) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(data.SerializeAsString());
  file_proto.set_md5_digest_hex_string(
      MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()));
  if (base::FeatureList::IsEnabled(kEnableTrustedVaultSHA256)) {
    file_proto.set_sha256_digest_hex_string(
        base::Base64Encode(crypto::hash::Sha256(
            base::as_byte_span(file_proto.serialized_local_trusted_vault()))));
  }
  bool success = base::ImportantFileWriter::WriteFileAtomically(
      file_path, file_proto.SerializeAsString(), "TrustedVault");
  if (!success) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
  base::UmaHistogramBoolean("TrustedVault.FileWriteSuccess." +
                                GetSecurityDomainNameForUma(security_domain_id),
                            success);
}

// Default file access logic StandaloneTrustedVaultStorage.
// Responsible for mapping per user / per security domain storage to files,
// and required data migrations.
class DefaultFileAccess : public StandaloneTrustedVaultStorage::FileAccess {
 public:
  DefaultFileAccess(const base::FilePath& base_dir,
                    SecurityDomainId security_domain_id)
      : file_path_(GetBackendFilePath(base_dir, security_domain_id)),
        security_domain_id_(security_domain_id) {}
  DefaultFileAccess(const DefaultFileAccess& other) = delete;
  DefaultFileAccess& operator=(const DefaultFileAccess& other) = delete;
  ~DefaultFileAccess() override = default;

  trusted_vault_pb::LocalTrustedVault ReadFromDisk() override {
    auto data = ReadDataFromDiskImpl(file_path_, security_domain_id_);

    if (data.user_size() == 0) {
      // No data, set the current version and omit writing the file.
      data.set_data_version(kCurrentLocalTrustedVaultVersion);
    }

    if (data.data_version() == 0) {
      UpgradeToVersion1(&data);
      WriteToDisk(data);
    }

    if (data.data_version() == 1) {
      UpgradeToVersion2(&data);
      WriteToDisk(data);
    }

    if (data.data_version() == 2) {
      UpgradeToVersion3(&data);
      WriteToDisk(data);
    }

    if (data.data_version() == 3) {
      UpgradeToVersion4(&data);
      WriteToDisk(data);
    }

    CHECK_EQ(data.data_version(), kCurrentLocalTrustedVaultVersion);

    return data;
  }

  void WriteToDisk(const trusted_vault_pb::LocalTrustedVault& data) override {
    WriteDataToDiskImpl(data, file_path_, security_domain_id_);
  }

 private:
  const base::FilePath file_path_;
  const SecurityDomainId security_domain_id_;
};

}  // namespace

std::unique_ptr<StandaloneTrustedVaultStorage>
StandaloneTrustedVaultStorage::CreateForTesting(
    std::unique_ptr<FileAccess> file_access) {
  return base::WrapUnique(
      new StandaloneTrustedVaultStorage(std::move(file_access)));
}

StandaloneTrustedVaultStorage::StandaloneTrustedVaultStorage(
    const base::FilePath& base_dir,
    SecurityDomainId security_domain_id)
    : file_access_(
          std::make_unique<DefaultFileAccess>(base_dir, security_domain_id)) {}

StandaloneTrustedVaultStorage::StandaloneTrustedVaultStorage(
    std::unique_ptr<FileAccess> file_access)
    : file_access_(std::move(file_access)) {
  CHECK(file_access_);
}

StandaloneTrustedVaultStorage::~StandaloneTrustedVaultStorage() = default;

trusted_vault_pb::LocalTrustedVaultPerUser*
StandaloneTrustedVaultStorage::AddUserVault(const GaiaId& gaia_id) {
  CHECK(FindUserVault(gaia_id) == nullptr);

  auto* user_vault = data_.add_user();
  user_vault->set_gaia_id(gaia_id.ToString());
  return user_vault;
}

trusted_vault_pb::LocalTrustedVaultPerUser*
StandaloneTrustedVaultStorage::FindUserVault(const GaiaId& gaia_id) {
  for (int i = 0; i < data_.user_size(); ++i) {
    if (GaiaId(data_.user(i).gaia_id()) == gaia_id) {
      return data_.mutable_user(i);
    }
  }
  return nullptr;
}

void StandaloneTrustedVaultStorage::RemoveUserVaults(
    base::FunctionRef<bool(const trusted_vault_pb::LocalTrustedVaultPerUser&)>
        predicate) {
  auto removed = std::ranges::remove_if(*data_.mutable_user(), predicate);
  data_.mutable_user()->erase(removed.begin(), removed.end());
}

void StandaloneTrustedVaultStorage::ReadDataFromDisk() {
  data_ = file_access_->ReadFromDisk();
}

void StandaloneTrustedVaultStorage::WriteDataToDisk() {
  file_access_->WriteToDisk(data_);
}

// static
bool StandaloneTrustedVaultStorage::HasNonConstantKey(
    const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault) {
  std::string constant_key_as_proto_string;
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           &constant_key_as_proto_string);
  for (const trusted_vault_pb::LocalTrustedVaultKey& key :
       per_user_vault.vault_key()) {
    if (key.key_material() != constant_key_as_proto_string) {
      return true;
    }
  }
  return false;
}

// static
std::vector<std::vector<uint8_t>>
StandaloneTrustedVaultStorage::GetAllVaultKeys(
    const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault) {
  std::vector<std::vector<uint8_t>> vault_keys;
  for (const trusted_vault_pb::LocalTrustedVaultKey& key :
       per_user_vault.vault_key()) {
    vault_keys.emplace_back(ProtoStringToBytes(key.key_material()));
  }
  return vault_keys;
}
}  // namespace trusted_vault
