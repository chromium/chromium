// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_storage_impl.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

namespace {

constexpr base::FilePath::CharType kChromeSyncTrustedVaultFilename[] =
    FILE_PATH_LITERAL("trusted_vault.pb");
constexpr base::FilePath::CharType kPasskeysTrustedVaultFilename[] =
    FILE_PATH_LITERAL("passkeys_trusted_vault.pb");

constexpr int kCurrentLocalTrustedVaultVersion = 3;

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

  if (base::MD5String(file_proto.serialized_local_trusted_vault()) !=
      file_proto.md5_digest_hex_string()) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kMD5DigestMismatch);
    return data_proto;
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
  DCHECK(local_trusted_vault);
  DCHECK_EQ(local_trusted_vault->data_version(), 0);

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
  DCHECK(local_trusted_vault);
  DCHECK_EQ(local_trusted_vault->data_version(), 1);

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
    local_trusted_vault->set_data_version(3);
  }
}

void WriteDataToDiskImpl(const trusted_vault_pb::LocalTrustedVault& data,
                         const base::FilePath& file_path,
                         SecurityDomainId security_domain_id) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(data.SerializeAsString());
  file_proto.set_md5_digest_hex_string(
      base::MD5String(file_proto.serialized_local_trusted_vault()));
  bool success = base::ImportantFileWriter::WriteFileAtomically(
      file_path, file_proto.SerializeAsString(), "TrustedVault");
  if (!success) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
  base::UmaHistogramBoolean("TrustedVault.FileWriteSuccess." +
                                GetSecurityDomainNameForUma(security_domain_id),
                            success);
}

}  // namespace

StandaloneTrustedVaultStorageImpl::StandaloneTrustedVaultStorageImpl(
    const base::FilePath& base_dir,
    SecurityDomainId security_domain_id)
    : file_path_(GetBackendFilePath(base_dir, security_domain_id)),
      security_domain_id_(security_domain_id) {}

StandaloneTrustedVaultStorageImpl::~StandaloneTrustedVaultStorageImpl() =
    default;

void StandaloneTrustedVaultStorageImpl::ReadDataFromDisk() {
  data_ = ReadDataFromDiskImpl(file_path_, security_domain_id_);

  if (data_.user_size() == 0) {
    // No data, set the current version and omit writing the file.
    data_.set_data_version(kCurrentLocalTrustedVaultVersion);
  }

  if (data_.data_version() == 0) {
    UpgradeToVersion1(&data_);
    WriteDataToDisk();
  }

  if (data_.data_version() == 1) {
    UpgradeToVersion2(&data_);
    WriteDataToDisk();
  }

  if (data_.data_version() == 2) {
    UpgradeToVersion3(&data_);
    WriteDataToDisk();
  }

  DCHECK_EQ(data_.data_version(), kCurrentLocalTrustedVaultVersion);
}

void StandaloneTrustedVaultStorageImpl::WriteDataToDisk() {
  WriteDataToDiskImpl(data_, file_path_, security_domain_id_);
}

trusted_vault_pb::LocalTrustedVaultPerUser*
StandaloneTrustedVaultStorageImpl::AddUserVault(const GaiaId& gaia_id) {
  DCHECK(FindUserVault(gaia_id) == nullptr);

  auto* user_vault = data_.add_user();
  user_vault->set_gaia_id(gaia_id.ToString());
  return user_vault;
}

trusted_vault_pb::LocalTrustedVaultPerUser*
StandaloneTrustedVaultStorageImpl::FindUserVault(const GaiaId& gaia_id) {
  for (int i = 0; i < data_.user_size(); ++i) {
    if (GaiaId(data_.user(i).gaia_id()) == gaia_id) {
      return data_.mutable_user(i);
    }
  }
  return nullptr;
}

void StandaloneTrustedVaultStorageImpl::RemoveUserVaults(
    base::FunctionRef<bool(const trusted_vault_pb::LocalTrustedVaultPerUser&)>
        predicate) {
  auto removed = std::ranges::remove_if(*data_.mutable_user(), predicate);
  data_.mutable_user()->erase(removed.begin(), removed.end());
}

}  // namespace trusted_vault
