// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/decryption.h"

#include <limits>
#include <string>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "crypto/aead.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace reporting {

Decryptor::Handle::Handle(base::StringPiece shared_secret,
                          scoped_refptr<Decryptor> decryptor)
    : shared_secret_(shared_secret), decryptor_(decryptor) {}

Decryptor::Handle::~Handle() = default;

void Decryptor::Handle::AddToRecord(base::StringPiece data,
                                    base::OnceCallback<void(Status)> cb) {
  // Add piece of data to the record.
  record_.append(data.data(), data.size());
  std::move(cb).Run(Status::StatusOK());
}

void Decryptor::Handle::CloseRecord(
    base::OnceCallback<void(StatusOr<base::StringPiece>)> cb) {
  // Make sure the record self-destructs when returning from this method.
  const auto self_destruct = base::WrapUnique(this);

  // Decrypt the data with symmetric key using AEAD interface.
  crypto::Aead aead(crypto::Aead::CHACHA20_POLY1305);

  // Produce symmetric key from shared secret using HKDF.
  // Since the original keys were only used once, no salt and context is needed.
  const auto out_symmetric_key = std::make_unique<uint8_t[]>(aead.KeyLength());
  if (!HKDF(out_symmetric_key.get(), aead.KeyLength(), /*digest=*/EVP_sha256(),
            reinterpret_cast<const uint8_t*>(shared_secret_.data()),
            shared_secret_.size(),
            /*salt=*/nullptr, /*salt_len=*/0,
            /*info=*/nullptr, /*info_len=*/0)) {
    std::move(cb).Run(
        Status(error::INTERNAL, "Symmetric key extraction failed"));
    return;
  }

  // Use the symmetric key for data decryption.
  aead.Init(base::make_span(out_symmetric_key.get(), aead.KeyLength()));

  // Set nonce to 0s, since a symmetric key is only used once.
  // Note: if we ever start reusing the same symmetric key, we will need
  // to generate new nonce for every record and transfer it to the peer.
  std::string nonce(aead.NonceLength(), 0);

  // Decrypt collected record.
  std::string decrypted;
  if (!aead.Open(record_, nonce, std::string(), &decrypted)) {
    std::move(cb).Run(Status(error::INTERNAL, "Failed to decrypt"));
    return;
  }
  record_.clear();  // Free unused memory.

  // Return decrypted record.
  std::move(cb).Run(decrypted);
}

void Decryptor::OpenRecord(base::StringPiece shared_secret,
                           base::OnceCallback<void(StatusOr<Handle*>)> cb) {
  std::move(cb).Run(new Handle(shared_secret, this));
}

StatusOr<std::string> Decryptor::DecryptSecret(
    base::StringPiece private_key,
    base::StringPiece peer_public_value) {
  // Verify the keys.
  if (private_key.size() != X25519_PRIVATE_KEY_LEN) {
    return Status(
        error::FAILED_PRECONDITION,
        base::StrCat({"Private key size mismatch, expected=",
                      base::NumberToString(X25519_PRIVATE_KEY_LEN),
                      " actual=", base::NumberToString(private_key.size())}));
  }
  if (peer_public_value.size() != X25519_PUBLIC_VALUE_LEN) {
    return Status(
        error::FAILED_PRECONDITION,
        base::StrCat({"Public key size mismatch, expected=",
                      base::NumberToString(X25519_PUBLIC_VALUE_LEN), " actual=",
                      base::NumberToString(peer_public_value.size())}));
  }

  // Compute shared secret.
  uint8_t out_shared_value[X25519_SHARED_KEY_LEN];
  if (!X25519(out_shared_value,
              reinterpret_cast<const uint8_t*>(private_key.data()),
              reinterpret_cast<const uint8_t*>(peer_public_value.data()))) {
    return Status(error::DATA_LOSS, "Curve25519 decryption failed");
  }

  return std::string(reinterpret_cast<const char*>(out_shared_value),
                     X25519_SHARED_KEY_LEN);
}

Decryptor::Decryptor()
    : keys_sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(keys_sequence_checker_);
}

Decryptor::~Decryptor() = default;

void Decryptor::RecordKeyPair(
    base::StringPiece private_key,
    base::StringPiece public_key,
    base::OnceCallback<void(StatusOr<Encryptor::PublicKeyId>)> cb) {
  // Schedule key recording on the sequenced task runner.
  keys_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::string public_key, KeyInfo key_info,
             base::OnceCallback<void(StatusOr<Encryptor::PublicKeyId>)> cb,
             scoped_refptr<Decryptor> decryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(decryptor->keys_sequence_checker_);
            StatusOr<Encryptor::PublicKeyId> result;
            if (key_info.private_key.size() != X25519_PRIVATE_KEY_LEN) {
              result = Status(
                  error::FAILED_PRECONDITION,
                  base::StrCat(
                      {"Private key size mismatch, expected=",
                       base::NumberToString(X25519_PRIVATE_KEY_LEN), " actual=",
                       base::NumberToString(key_info.private_key.size())}));
            } else if (public_key.size() != X25519_PUBLIC_VALUE_LEN) {
              result = Status(
                  error::FAILED_PRECONDITION,
                  base::StrCat(
                      {"Public key size mismatch, expected=",
                       base::NumberToString(X25519_PUBLIC_VALUE_LEN),
                       " actual=", base::NumberToString(public_key.size())}));
            } else {
              // Assign a random number to be public key id for testing purposes
              // only (in production it will be retrieved from the server as
              // 'int32').
              const Encryptor::PublicKeyId public_key_id = base::RandGenerator(
                  std::numeric_limits<Encryptor::PublicKeyId>::max());
              if (!decryptor->keys_.emplace(public_key_id, key_info).second) {
                result = Status(error::ALREADY_EXISTS,
                                base::StrCat({"Public key='", public_key,
                                              "' already recorded"}));
              } else {
                result = public_key_id;
              }
            }
            // Schedule response on a generic thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE, base::BindOnce(
                               [](base::OnceCallback<void(
                                      StatusOr<Encryptor::PublicKeyId>)> cb,
                                  StatusOr<Encryptor::PublicKeyId> result) {
                                 std::move(cb).Run(result);
                               },
                               std::move(cb), result));
          },
          std::string(public_key),
          KeyInfo{.private_key = std::string(private_key),
                  .time_stamp = base::Time::Now()},
          std::move(cb), base::WrapRefCounted(this)));
}

void Decryptor::RetrieveMatchingPrivateKey(
    Encryptor::PublicKeyId public_key_id,
    base::OnceCallback<void(StatusOr<std::string>)> cb) {
  // Schedule key retrieval on the sequenced task runner.
  keys_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Encryptor::PublicKeyId public_key_id,
             base::OnceCallback<void(StatusOr<std::string>)> cb,
             scoped_refptr<Decryptor> decryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(decryptor->keys_sequence_checker_);
            auto key_info_it = decryptor->keys_.find(public_key_id);
            if (key_info_it != decryptor->keys_.end()) {
              DCHECK_EQ(key_info_it->second.private_key.size(),
                        static_cast<size_t>(X25519_PRIVATE_KEY_LEN));
            }
            // Schedule response on a generic thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<void(StatusOr<std::string>)> cb,
                       StatusOr<std::string> result) {
                      std::move(cb).Run(result);
                    },
                    std::move(cb),
                    key_info_it == decryptor->keys_.end()
                        ? StatusOr<std::string>(Status(
                              error::NOT_FOUND, "Matching key not found"))
                        : key_info_it->second.private_key));
          },
          public_key_id, std::move(cb), base::WrapRefCounted(this)));
}

StatusOr<scoped_refptr<Decryptor>> Decryptor::Create() {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();
  return base::WrapRefCounted(new Decryptor());
}

}  // namespace reporting
