// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/encryption.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

Encryptor::Handle::Handle(scoped_refptr<Encryptor> encryptor)
    : encryptor_(encryptor) {}

Encryptor::Handle::~Handle() = default;

void Encryptor::Handle::AddToRecord(std::string_view data,
                                    base::OnceCallback<void(Status)> cb) {
  // Append new data to the record.
  record_.append(data);
  std::move(cb).Run(Status::StatusOK());
}

void Encryptor::Handle::CloseRecord(
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
  // Retrieves asymmetric public key to use.
  encryptor_->RetrieveAsymmetricKey(base::BindOnce(
      &Handle::ProduceEncryptedRecord, base::Unretained(this), std::move(cb)));
}

void Encryptor::Handle::ProduceEncryptedRecord(
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
    StatusOr<std::pair<std::string, PublicKeyId>> asymmetric_key_result) {
  // Make sure the record self-destructs when returning from this method.
  const auto self_destruct = base::WrapUnique(this);

  // Validate and accept asymmetric peer key.
  if (!asymmetric_key_result.has_value()) {
    std::move(cb).Run(
        base::unexpected(std::move(asymmetric_key_result).error()));
    return;
  }
  const auto& asymmetric_key = asymmetric_key_result.value();
  if (asymmetric_key.first.size() != kKeySize) {
    std::move(cb).Run(base::unexpected(Status(
        error::INTERNAL,
        base::StrCat({"Asymmetric key size mismatch, expected=",
                      base::NumberToString(kKeySize), " actual=",
                      base::NumberToString(asymmetric_key.first.size())}))));
    return;
  }

  // Prepare encrypted record.
  EncryptedRecord encrypted_record;
  encrypted_record.mutable_encryption_info()->set_public_key_id(
      asymmetric_key.second);
  encrypted_record.mutable_encryption_info()->mutable_encryption_key()->resize(
      kKeySize);

  // Compute shared secret, store it in |encrypted_record|.
  uint8_t out_shared_secret[kKeySize];
  uint8_t out_generatet_public_value[kKeySize];
  if (!ComputeSharedSecret(
          reinterpret_cast<const uint8_t*>(asymmetric_key.first.data()),
          out_shared_secret, out_generatet_public_value)) {
    std::move(cb).Run(base::unexpected(
        Status(error::DATA_LOSS, "Curve25519 shared secret not derived")));
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::FAILED_TO_CREATE_ENCRYPTION_KEY,
        DataLossErrorReason::MAX_VALUE);
    return;
  }
  encrypted_record.mutable_encryption_info()->mutable_encryption_key()->assign(
      reinterpret_cast<const char*>(out_generatet_public_value), kKeySize);

  // Produce symmetric key from shared secret using HKDF.
  uint8_t out_symmetric_key[kKeySize];
  if (!ProduceSymmetricKey(out_shared_secret, out_symmetric_key)) {
    std::move(cb).Run(base::unexpected(
        Status(error::INTERNAL, "Symmetric key production failed")));
    return;
  }

  // Perform symmmetric encryption with the shared secret as a Chacha20Poly1305
  // key and place result in |encrypted_record|.
  if (!PerformSymmetricEncryption(
          out_symmetric_key, record_,
          encrypted_record.mutable_encrypted_wrapped_record())) {
    std::move(cb).Run(base::unexpected(
        Status(error::INTERNAL, "Symmetric encryption failed")));
    return;
  }
  record_.clear();  // Free unused memory.

  // Return EncryptedRecord.
  std::move(cb).Run(encrypted_record);
}

Encryptor::Encryptor()
    : asymmetric_key_sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(asymmetric_key_sequence_checker_);
}

Encryptor::~Encryptor() = default;

void Encryptor::UpdateAsymmetricKey(
    std::string_view new_public_key,
    PublicKeyId new_public_key_id,
    base::OnceCallback<void(Status)> response_cb) {
  if (new_public_key.empty()) {
    std::move(response_cb)
        .Run(Status(error::INVALID_ARGUMENT, "Provided key is empty"));
    return;
  }

  // Schedule key update on the sequenced task runner.
  asymmetric_key_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::string_view new_public_key, PublicKeyId new_public_key_id,
             scoped_refptr<Encryptor> encryptor) {
            encryptor->asymmetric_key_ =
                std::make_pair(std::string(new_public_key), new_public_key_id);
          },
          std::string(new_public_key), new_public_key_id,
          base::WrapRefCounted(this)));

  // Response OK not waiting for the update.
  std::move(response_cb).Run(Status::StatusOK());
}

void Encryptor::OpenRecord(base::OnceCallback<void(StatusOr<Handle*>)> cb) {
  std::move(cb).Run(new Handle(this));
}

void Encryptor::RetrieveAsymmetricKey(
    base::OnceCallback<void(StatusOr<std::pair<std::string, PublicKeyId>>)>
        cb) {
  // Schedule key retrieval on the sequenced task runner.
  asymmetric_key_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(
                 StatusOr<std::pair<std::string, PublicKeyId>>)> cb,
             scoped_refptr<Encryptor> encryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(
                encryptor->asymmetric_key_sequence_checker_);
            // Schedule response on regular thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<void(
                           StatusOr<std::pair<std::string, PublicKeyId>>)> cb,
                       StatusOr<std::pair<std::string, PublicKeyId>> response) {
                      std::move(cb).Run(std::move(response));
                    },
                    std::move(cb),
                    !encryptor->asymmetric_key_.has_value()
                        ? StatusOr<std::pair<std::string, PublicKeyId>>(
                              base::unexpected(Status(
                                  error::NOT_FOUND, "Asymmetric key not set")))
                        : encryptor->asymmetric_key_.value()));
          },
          std::move(cb), base::WrapRefCounted(this)));
}

StatusOr<scoped_refptr<Encryptor>> Encryptor::Create() {
  return base::WrapRefCounted(new Encryptor());
}

}  // namespace reporting
