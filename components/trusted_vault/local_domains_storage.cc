// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/local_domains_storage.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/memory/ptr_util.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace trusted_vault {

namespace {

constexpr base::FilePath::CharType kLocalDomainsFileName[] =
    FILE_PATH_LITERAL("local_domains_data.pb");

constexpr int kCurrentLocalDomainsDataVersion = 1;

trusted_vault_pb::LocalDomainsData ReadLocalDomainsDataFromDiskImpl(
    const base::FilePath& file_path) {
  // TODO(crbug.com/542893243): Use OS Crypt to decrypt the file content.
  trusted_vault_pb::LocalDomainsData data_proto;
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    return trusted_vault_pb::LocalDomainsData();
  }

  if (!data_proto.ParseFromString(file_content)) {
    return trusted_vault_pb::LocalDomainsData();
  }

  if (data_proto.data_version() != kCurrentLocalDomainsDataVersion) {
    return trusted_vault_pb::LocalDomainsData();
  }

  return data_proto;
}

bool WriteLocalDomainsDataToDiskImpl(
    const trusted_vault_pb::LocalDomainsData& data,
    const base::FilePath& file_path) {
  // TODO(crbug.com/542893243): Use OS Crypt to encrypt the file content.
  return base::ImportantFileWriter::WriteFileAtomically(
      file_path, data.SerializeAsString(), "TrustedVault");
}

// Default file access implementation for `LocalDomainsStorage`.
// Responsible for reading/writing `local_domains_data.pb`.
class DefaultStorageFileAccess : public LocalDomainsStorage::StorageFileAccess {
 public:
  explicit DefaultStorageFileAccess(const base::FilePath& base_dir)
      : local_domains_file_path_(base_dir.Append(kLocalDomainsFileName)) {}
  DefaultStorageFileAccess(const DefaultStorageFileAccess&) = delete;
  DefaultStorageFileAccess& operator=(const DefaultStorageFileAccess&) = delete;
  ~DefaultStorageFileAccess() override = default;

  trusted_vault_pb::LocalDomainsData ReadFromDisk() override {
    if (!base::PathExists(local_domains_file_path_)) {
      trusted_vault_pb::LocalDomainsData data;
      data.set_data_version(kCurrentLocalDomainsDataVersion);
      return data;
    }

    trusted_vault_pb::LocalDomainsData data =
        ReadLocalDomainsDataFromDiskImpl(local_domains_file_path_);
    if (data.data_version() != kCurrentLocalDomainsDataVersion) {
      data.Clear();
      data.set_data_version(kCurrentLocalDomainsDataVersion);
    }
    return data;
  }

  void WriteToDisk(const trusted_vault_pb::LocalDomainsData& data) override {
    WriteLocalDomainsDataToDiskImpl(data, local_domains_file_path_);
  }

 private:
  const base::FilePath local_domains_file_path_;
};

}  // namespace

// static
std::unique_ptr<LocalDomainsStorage> LocalDomainsStorage::Create(
    const base::FilePath& base_dir) {
  return base::WrapUnique(new LocalDomainsStorage(
      std::make_unique<DefaultStorageFileAccess>(base_dir)));
}

// static
std::unique_ptr<LocalDomainsStorage> LocalDomainsStorage::CreateForTesting(
    std::unique_ptr<StorageFileAccess> file_access) {
  return base::WrapUnique(new LocalDomainsStorage(std::move(file_access)));
}

LocalDomainsStorage::LocalDomainsStorage(
    std::unique_ptr<StorageFileAccess> file_access)
    : file_access_(std::move(file_access)) {
  CHECK(file_access_);
}

LocalDomainsStorage::~LocalDomainsStorage() = default;

void LocalDomainsStorage::ReadDataFromDisk() {
  data_ = file_access_->ReadFromDisk();
}

void LocalDomainsStorage::WriteToDisk() {
  file_access_->WriteToDisk(data_);
}

trusted_vault_pb::UserDomainData* LocalDomainsStorage::FindUser(
    const GaiaId& gaia_id) {
  for (auto& user : *data_.mutable_user()) {
    if (user.gaia_id() == gaia_id.ToString()) {
      return &user;
    }
  }
  return nullptr;
}

const trusted_vault_pb::UserDomainData* LocalDomainsStorage::FindUser(
    const GaiaId& gaia_id) const {
  for (const trusted_vault_pb::UserDomainData& user : data_.user()) {
    if (user.gaia_id() == gaia_id.ToString()) {
      return &user;
    }
  }
  return nullptr;
}

trusted_vault_pb::UserDomainData* LocalDomainsStorage::GetOrCreateUser(
    const GaiaId& gaia_id) {
  trusted_vault_pb::UserDomainData* user = FindUser(gaia_id);
  if (!user) {
    user = data_.add_user();
    user->set_gaia_id(gaia_id.ToString());
  }
  return user;
}

trusted_vault_pb::DomainData* LocalDomainsStorage::FindDomainData(
    trusted_vault_pb::UserDomainData* user,
    SecurityDomainId domain) {
  if (!user) {
    return nullptr;
  }
  const int32_t domain_id = static_cast<int32_t>(domain);
  for (auto& domain_data : *user->mutable_domain_data()) {
    if (domain_data.domain_id() == domain_id) {
      return &domain_data;
    }
  }
  return nullptr;
}

const trusted_vault_pb::DomainData* LocalDomainsStorage::FindDomainData(
    const trusted_vault_pb::UserDomainData* user,
    SecurityDomainId domain) const {
  if (!user) {
    return nullptr;
  }
  const int32_t domain_id = static_cast<int32_t>(domain);
  for (const trusted_vault_pb::DomainData& domain_data : user->domain_data()) {
    if (domain_data.domain_id() == domain_id) {
      return &domain_data;
    }
  }
  return nullptr;
}

trusted_vault_pb::DomainData* LocalDomainsStorage::GetOrCreateDomainData(
    trusted_vault_pb::UserDomainData* user,
    SecurityDomainId domain) {
  CHECK(user);
  trusted_vault_pb::DomainData* domain_data = FindDomainData(user, domain);
  if (!domain_data) {
    domain_data = user->add_domain_data();
    domain_data->set_domain_id(static_cast<int32_t>(domain));
  }
  return domain_data;
}

void LocalDomainsStorage::RemoveUsers(
    base::FunctionRef<bool(const trusted_vault_pb::UserDomainData&)>
        predicate) {
  if (google::protobuf::erase_if(*data_.mutable_user(), predicate) > 0) {
    WriteToDisk();
  }
}

void LocalDomainsStorage::MutateUser(
    const GaiaId& gaia_id,
    base::FunctionRef<void(trusted_vault_pb::UserDomainData&)> mutator) {
  trusted_vault_pb::UserDomainData* user = GetOrCreateUser(gaia_id);
  mutator(*user);
  WriteToDisk();
}

void LocalDomainsStorage::MutateDomain(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    base::FunctionRef<void(trusted_vault_pb::DomainData&)> mutator) {
  trusted_vault_pb::UserDomainData* user = GetOrCreateUser(gaia_id);
  trusted_vault_pb::DomainData* domain_data =
      GetOrCreateDomainData(user, domain);
  mutator(*domain_data);
  WriteToDisk();
}

void LocalDomainsStorage::ClearDataForUser(const GaiaId& gaia_id) {
  RemoveUsers([&](const trusted_vault_pb::UserDomainData& user) {
    return user.gaia_id() == gaia_id.ToString();
  });
}

void LocalDomainsStorage::ClearDataForUnknownUsersOrMarkForDeletion(
    const base::flat_set<GaiaId>& known_gaia_ids,
    const std::optional<GaiaId>& primary_account_gaia_id) {
  bool should_write_data = false;
  if (primary_account_gaia_id.has_value() &&
      !known_gaia_ids.contains(*primary_account_gaia_id)) {
    trusted_vault_pb::UserDomainData* user =
        GetOrCreateUser(*primary_account_gaia_id);
    if (!user->should_delete_keys_when_non_primary()) {
      user->set_should_delete_keys_when_non_primary(true);
      should_write_data = true;
    }
  }

  if (google::protobuf::erase_if(
          *data_.mutable_user(),
          [&](const trusted_vault_pb::UserDomainData& user) {
            const GaiaId gaia_id(user.gaia_id());
            if (primary_account_gaia_id.has_value() &&
                gaia_id == *primary_account_gaia_id) {
              return false;
            }
            return !known_gaia_ids.contains(gaia_id);
          }) > 0) {
    should_write_data = true;
  }

  if (should_write_data) {
    WriteToDisk();
  }
}

void LocalDomainsStorage::ClearDataForUsersMarkedForDeletion(
    const std::optional<GaiaId>& primary_account_gaia_id) {
  RemoveUsers([&](const trusted_vault_pb::UserDomainData& user) {
    return user.should_delete_keys_when_non_primary() &&
           (!primary_account_gaia_id.has_value() ||
            *primary_account_gaia_id != GaiaId(user.gaia_id()));
  });
}

PhysicalDeviceRecoveryFactorData
LocalDomainsStorage::GetPhysicalDeviceRecoveryFactorData(
    const GaiaId& gaia_id) const {
  const auto* user = FindUser(gaia_id);
  if (!user) {
    return PhysicalDeviceRecoveryFactorData();
  }
  return user->physical_device_data();
}

void LocalDomainsStorage::MutatePhysicalDeviceRecoveryFactorData(
    const GaiaId& gaia_id,
    base::FunctionRef<void(PhysicalDeviceRecoveryFactorData&)> mutator) {
  MutateUser(gaia_id, [&](trusted_vault_pb::UserDomainData& user) {
    mutator(*user.mutable_physical_device_data());
  });
}

bool LocalDomainsStorage::IsRecoveryFactorRegistered(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    LocalRecoveryFactorType factor_type) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  if (!domain_data) {
    return false;
  }
  const int32_t type_val = static_cast<int32_t>(factor_type);
  for (const auto& factor : domain_data->recovery_factor_registration_info()) {
    if (factor.factor_type() == type_val) {
      return factor.registered();
    }
  }
  return false;
}

void LocalDomainsStorage::SetRecoveryFactorRegistered(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    LocalRecoveryFactorType factor_type,
    bool registered) {
  MutateDomain(gaia_id, domain, [&](trusted_vault_pb::DomainData& domain_data) {
    const int32_t type_val = static_cast<int32_t>(factor_type);
    trusted_vault_pb::RecoveryFactorRegistrationInfo* factor_info = nullptr;
    for (auto& info :
         *domain_data.mutable_recovery_factor_registration_info()) {
      if (info.factor_type() == type_val) {
        factor_info = &info;
        break;
      }
    }
    if (!factor_info) {
      factor_info = domain_data.add_recovery_factor_registration_info();
      factor_info->set_factor_type(type_val);
    }
    factor_info->set_registered(registered);
  });
}

bool LocalDomainsStorage::GetLastRegistrationReturnedLocalDataObsolete(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  if (!user) {
    return false;
  }
  const auto* domain_data = FindDomainData(user, domain);
  return domain_data &&
         domain_data->last_registration_returned_local_data_obsolete();
}

void LocalDomainsStorage::SetLastRegistrationReturnedLocalDataObsolete(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    bool obsolete) {
  MutateDomain(gaia_id, domain,
               [obsolete](trusted_vault_pb::DomainData& domain_data) {
                 domain_data.set_last_registration_returned_local_data_obsolete(
                     obsolete);
               });
}

std::vector<std::vector<uint8_t>> LocalDomainsStorage::GetVaultKeys(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  if (!domain_data) {
    return {};
  }
  std::vector<std::vector<uint8_t>> result;
  for (const auto& key : domain_data->vault_key()) {
    result.emplace_back(ProtoStringToBytes(key.key_material()));
  }
  return result;
}

int LocalDomainsStorage::GetLastKeyVersion(const GaiaId& gaia_id,
                                           SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  return domain_data ? domain_data->last_vault_key_version() : 0;
}

void LocalDomainsStorage::SetVaultKeys(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  MutateDomain(gaia_id, domain, [&](trusted_vault_pb::DomainData& domain_data) {
    domain_data.set_last_vault_key_version(last_key_version);
    domain_data.set_keys_marked_as_stale_by_consumer(false);
    domain_data.clear_vault_key();
    for (const auto& key : keys) {
      AssignBytesToProtoString(
          key, domain_data.add_vault_key()->mutable_key_material());
    }
  });
}

bool LocalDomainsStorage::GetKeysMarkedAsStaleByConsumer(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  return domain_data && domain_data->keys_marked_as_stale_by_consumer();
}

void LocalDomainsStorage::SetKeysMarkedAsStaleByConsumer(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    bool keys_marked_as_stale) {
  MutateDomain(
      gaia_id, domain,
      [keys_marked_as_stale](trusted_vault_pb::DomainData& domain_data) {
        domain_data.set_keys_marked_as_stale_by_consumer(keys_marked_as_stale);
      });
}

bool LocalDomainsStorage::HasNonConstantKey(const GaiaId& gaia_id,
                                            SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  if (!domain_data) {
    return false;
  }
  for (const auto& key : domain_data->vault_key()) {
    if (ProtoStringToBytes(key.key_material()) !=
        GetConstantTrustedVaultKey()) {
      return true;
    }
  }
  return false;
}

int64_t LocalDomainsStorage::GetLastFailedRequestMillis(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  if (!domain_data ||
      !domain_data->has_last_failed_request_millis_since_unix_epoch()) {
    return 0;
  }
  return domain_data->last_failed_request_millis_since_unix_epoch();
}

void LocalDomainsStorage::SetLastFailedRequestMillis(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    int64_t last_failed_request_millis) {
  MutateDomain(
      gaia_id, domain,
      [last_failed_request_millis](trusted_vault_pb::DomainData& domain_data) {
        domain_data.set_last_failed_request_millis_since_unix_epoch(
            last_failed_request_millis);
      });
}

trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
LocalDomainsStorage::GetDegradedRecoverabilityState(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  const auto* user = FindUser(gaia_id);
  const auto* domain_data = FindDomainData(user, domain);
  if (!domain_data || !domain_data->has_degraded_recoverability_state()) {
    return trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState();
  }
  return domain_data->degraded_recoverability_state();
}

void LocalDomainsStorage::SetDegradedRecoverabilityState(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
        state) {
  MutateDomain(gaia_id, domain,
               [&state](trusted_vault_pb::DomainData& domain_data) {
                 *domain_data.mutable_degraded_recoverability_state() = state;
               });
}

}  // namespace trusted_vault
