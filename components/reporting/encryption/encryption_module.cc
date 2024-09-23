// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/encryption_module.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

namespace {

// Helper function for asynchronous encryption.
void AddToRecord(std::string_view record,
                 Encryptor::Handle* handle,
                 base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
  handle->AddToRecord(
      record,
      base::BindOnce(
          [](Encryptor::Handle* handle,
             base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
             Status status) {
            if (!status.ok()) {
              std::move(cb).Run(base::unexpected(std::move(status)));
              return;
            }
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(&Encryptor::Handle::CloseRecord,
                               base::Unretained(handle), std::move(cb)));
          },
          base::Unretained(handle), std::move(cb)));
}

}  // namespace

EncryptionModule::EncryptionModule(base::TimeDelta renew_encryption_key_period)
    : EncryptionModuleInterface(renew_encryption_key_period) {
  static_assert(std::is_same<PublicKeyId, Encryptor::PublicKeyId>::value,
                "Public key id types must match");
  auto encryptor_result = Encryptor::Create();
  CHECK(encryptor_result.has_value()) << encryptor_result.error();
  encryptor_ = std::move(encryptor_result.value());
}

EncryptionModule::~EncryptionModule() = default;

void EncryptionModule::EncryptRecordImpl(
    std::string_view record,
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) const {
  // Encryption key is available, encrypt.
  encryptor_->OpenRecord(base::BindOnce(
      [](std::string record,
         base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
         StatusOr<Encryptor::Handle*> handle_result) {
        if (!handle_result.has_value()) {
          std::move(cb).Run(base::unexpected(std::move(handle_result).error()));
          return;
        }
        base::ThreadPool::PostTask(
            FROM_HERE, base::BindOnce(&AddToRecord, record,
                                      base::Unretained(handle_result.value()),
                                      std::move(cb)));
      },
      std::string(record), std::move(cb)));
}

void EncryptionModule::UpdateAsymmetricKeyImpl(
    std::string_view new_public_key,
    PublicKeyId new_public_key_id,
    base::OnceCallback<void(Status)> response_cb) {
  encryptor_->UpdateAsymmetricKey(new_public_key, new_public_key_id,
                                  std::move(response_cb));
}

// static
scoped_refptr<EncryptionModuleInterface> EncryptionModule::Create(
    base::TimeDelta renew_encryption_key_period) {
  return base::WrapRefCounted(
      new EncryptionModule(renew_encryption_key_period));
}

}  // namespace reporting
