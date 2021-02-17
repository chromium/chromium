// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/encryption_module.h"

#include <atomic>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

namespace {

// Temporary: enable/disable encryption.
const base::Feature kEncryptedReportingFeature{
    EncryptionModule::kEncryptedReporting, base::FEATURE_DISABLED_BY_DEFAULT};

// Helper function for asynchronous encryption.
void AddToRecord(base::StringPiece record,
                 Encryptor::Handle* handle,
                 base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
  handle->AddToRecord(
      record,
      base::BindOnce(
          [](Encryptor::Handle* handle,
             base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
             Status status) {
            if (!status.ok()) {
              std::move(cb).Run(status);
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

// static
const char EncryptionModule::kEncryptedReporting[] = "EncryptedReporting";

// static
bool EncryptionModule::is_enabled() {
  return base::FeatureList::IsEnabled(kEncryptedReportingFeature);
}

EncryptionModule::EncryptionModule(base::TimeDelta renew_encryption_key_period)
    : renew_encryption_key_period_(renew_encryption_key_period) {
  auto encryptor_result = Encryptor::Create();
  DCHECK(encryptor_result.ok());
  encryptor_ = std::move(encryptor_result.ValueOrDie());
}

EncryptionModule::~EncryptionModule() = default;

void EncryptionModule::EncryptRecord(
    base::StringPiece record,
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) const {
  if (!is_enabled()) {
    // Encryptor disabled.
    EncryptedRecord encrypted_record;
    encrypted_record.mutable_encrypted_wrapped_record()->assign(record.begin(),
                                                                record.end());
    // encryption_info is not set.
    std::move(cb).Run(std::move(encrypted_record));
    return;
  }

  // Encryptor enabled: start encryption of the record as a whole.
  if (!has_encryption_key()) {
    // Encryption key is not available.
    std::move(cb).Run(
        Status(error::NOT_FOUND, "Cannot encrypt record - no key"));
    return;
  }
  // Encryption key is available, encrypt.
  encryptor_->OpenRecord(base::BindOnce(
      [](base::StringPiece record,
         base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
         StatusOr<Encryptor::Handle*> handle_result) {
        if (!handle_result.ok()) {
          std::move(cb).Run(handle_result.status());
          return;
        }
        base::ThreadPool::PostTask(
            FROM_HERE,
            base::BindOnce(&AddToRecord, std::string(record),
                           base::Unretained(handle_result.ValueOrDie()),
                           std::move(cb)));
      },
      std::string(record), std::move(cb)));
}

void EncryptionModule::UpdateAsymmetricKey(
    base::StringPiece new_public_key,
    Encryptor::PublicKeyId new_public_key_id,
    base::OnceCallback<void(Status)> response_cb) {
  encryptor_->UpdateAsymmetricKey(
      new_public_key, new_public_key_id,
      base::BindOnce(
          [](EncryptionModule* encryption_module,
             base::OnceCallback<void(Status)> response_cb, Status status) {
            if (status.ok()) {
              encryption_module->last_encryption_key_update_.store(
                  base::TimeTicks::Now());
            }
            std::move(response_cb).Run(status);
          },
          base::Unretained(this), std::move(response_cb)));
}

bool EncryptionModule::has_encryption_key() const {
  return !last_encryption_key_update_.load().is_null();
}

bool EncryptionModule::need_encryption_key() const {
  return !has_encryption_key() ||
         last_encryption_key_update_.load() + renew_encryption_key_period_ <
             base::TimeTicks::Now();
}

}  // namespace reporting
