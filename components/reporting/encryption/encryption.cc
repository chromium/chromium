// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/encryption.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "crypto/aead.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace reporting {

Encryptor::Handle::Handle(scoped_refptr<Encryptor> encryptor)
    : encryptor_(encryptor) {}

Encryptor::Handle::~Handle() = default;

void Encryptor::Handle::AddToRecord(base::StringPiece data,
                                    base::OnceCallback<void(Status)> cb) {
  // Append new data to the record.
  record_.append(data.data(), data.size());
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

  // Validate keys.
  if (!asymmetric_key_result.ok()) {
    std::move(cb).Run(asymmetric_key_result.status());
    return;
  }
  const auto& asymmetric_key = asymmetric_key_result.ValueOrDie();
  if (asymmetric_key.first.size() != X25519_PUBLIC_VALUE_LEN) {
    std::move(cb).Run(Status(
        error::INTERNAL,
        base::StrCat({"Asymmetric key size mismatch, expected=",
                      base::NumberToString(X25519_PUBLIC_VALUE_LEN), " actual=",
                      base::NumberToString(asymmetric_key.first.size())})));
    return;
  }

  // Generate new pair of private key and public value.
  uint8_t out_public_value[X25519_PUBLIC_VALUE_LEN];
  uint8_t out_private_key[X25519_PRIVATE_KEY_LEN];
  X25519_keypair(out_public_value, out_private_key);

  // Compute shared secret.
  uint8_t out_shared_secret[X25519_SHARED_KEY_LEN];
  if (!X25519(out_shared_secret, out_private_key,
              reinterpret_cast<const uint8_t*>(asymmetric_key.first.data()))) {
    std::move(cb).Run(Status(error::DATA_LOSS, "Curve25519 encryption failed"));
    return;
  }

  // Encrypt the data with symmetric key using AEAD interface.
  crypto::Aead aead(crypto::Aead::CHACHA20_POLY1305);

  // Produce symmetric key from shared secret using HKDF.
  // Since the keys above are only used once, no salt and context is provided.
  const auto out_symmetric_key = std::make_unique<uint8_t[]>(aead.KeyLength());
  if (!HKDF(out_symmetric_key.get(), aead.KeyLength(), /*digest=*/EVP_sha256(),
            out_shared_secret, X25519_SHARED_KEY_LEN,
            /*salt=*/nullptr, /*salt_len=*/0,
            /*info=*/nullptr, /*info_len=*/0)) {
    std::move(cb).Run(
        Status(error::INTERNAL, "Symmetric key extraction failed"));
    return;
  }

  // Use the symmetric key for data encryption.
  aead.Init(base::make_span(out_symmetric_key.get(), aead.KeyLength()));

  // Set nonce to 0s, since a symmetric key is only used once.
  // Note: if we ever start reusing the same symmetric key, we will need
  // to generate new nonce for every record and transfer it to the peer.
  std::string nonce(aead.NonceLength(), 0);

  // Prepare encrypted record.
  EncryptedRecord encrypted_record;
  encrypted_record.mutable_encryption_info()->set_public_key_id(
      asymmetric_key.second);
  encrypted_record.mutable_encryption_info()->set_encryption_key(
      reinterpret_cast<const char*>(out_public_value), X25519_PUBLIC_VALUE_LEN);

  // Encrypt the whole record.
  if (!aead.Seal(record_, nonce, std::string(),
                 encrypted_record.mutable_encrypted_wrapped_record()) ||
      encrypted_record.encrypted_wrapped_record().empty()) {
    std::move(cb).Run(Status(error::INTERNAL, "Failed to encrypt the record"));
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
    base::StringPiece new_public_key,
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
          [](base::StringPiece new_public_key, PublicKeyId new_public_key_id,
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
            StatusOr<std::pair<std::string, PublicKeyId>> response;
            // Schedule response on regular thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<void(
                           StatusOr<std::pair<std::string, PublicKeyId>>)> cb,
                       StatusOr<std::pair<std::string, PublicKeyId>> response) {
                      std::move(cb).Run(response);
                    },
                    std::move(cb),
                    !encryptor->asymmetric_key_.has_value()
                        ? StatusOr<std::pair<std::string, PublicKeyId>>(Status(
                              error::NOT_FOUND, "Asymmetric key not set"))
                        : encryptor->asymmetric_key_.value()));
          },
          std::move(cb), base::WrapRefCounted(this)));
}

StatusOr<scoped_refptr<Encryptor>> Encryptor::Create() {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();
  return base::WrapRefCounted(new Encryptor());
}

}  // namespace reporting
