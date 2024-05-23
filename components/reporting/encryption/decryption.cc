// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/decryption.h"

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {
namespace test {

Decryptor::Handle::Handle(std::string_view shared_secret,
                          scoped_refptr<Decryptor> decryptor)
    : shared_secret_(shared_secret), decryptor_(decryptor) {}

Decryptor::Handle::~Handle() = default;

void Decryptor::Handle::AddToRecord(std::string_view data,
                                    base::OnceCallback<void(Status)> cb) {
  // Add piece of data to the record.
  record_.append(data);
  std::move(cb).Run(Status::StatusOK());
}

void Decryptor::Handle::CloseRecord(
    base::OnceCallback<void(StatusOr<std::string_view>)> cb) {
  // Make sure the record self-destructs when returning from this method.
  const auto self_destruct = base::WrapUnique(this);

  // Produce symmetric key from shared secret using HKDF.
  // Since the original keys were only used once, no salt and context is needed.
  uint8_t out_symmetric_key[kKeySize];
  if (!ProduceSymmetricKey(
          reinterpret_cast<const uint8_t*>(shared_secret_.data()),
          out_symmetric_key)) {
    std::move(cb).Run(base::unexpected(
        Status(error::INTERNAL, "Symmetric key extraction failed")));
    return;
  }

  std::string decrypted;
  PerformSymmetricDecryption(out_symmetric_key, record_, &decrypted);
  record_.clear();

  // Return decrypted record.
  std::move(cb).Run(decrypted);
}

void Decryptor::OpenRecord(std::string_view shared_secret,
                           base::OnceCallback<void(StatusOr<Handle*>)> cb) {
  std::move(cb).Run(new Handle(shared_secret, this));
}

StatusOr<std::string> Decryptor::DecryptSecret(
    std::string_view private_key,
    std::string_view peer_public_value) {
  // Verify the keys.
  if (private_key.size() != kKeySize) {
    return base::unexpected(Status(
        error::FAILED_PRECONDITION,
        base::StrCat({"Private key size mismatch, expected=",
                      base::NumberToString(kKeySize),
                      " actual=", base::NumberToString(private_key.size())})));
  }
  if (peer_public_value.size() != kKeySize) {
    return base::unexpected(
        Status(error::FAILED_PRECONDITION,
               base::StrCat({"Public key size mismatch, expected=",
                             base::NumberToString(kKeySize), " actual=",
                             base::NumberToString(peer_public_value.size())})));
  }

  // Compute shared secret.
  uint8_t out_shared_value[kKeySize];
  RestoreSharedSecret(
      reinterpret_cast<const uint8_t*>(private_key.data()),
      reinterpret_cast<const uint8_t*>(peer_public_value.data()),
      out_shared_value);

  return std::string(reinterpret_cast<const char*>(out_shared_value), kKeySize);
}

Decryptor::Decryptor()
    : keys_sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(keys_sequence_checker_);
}

Decryptor::~Decryptor() = default;

void Decryptor::RecordKeyPair(
    std::string_view private_key,
    std::string_view public_key,
    base::OnceCallback<void(StatusOr<Encryptor::PublicKeyId>)> cb) {
  // Schedule key recording on the sequenced task runner.
  keys_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::string public_key, KeyInfo key_info,
             base::OnceCallback<void(StatusOr<Encryptor::PublicKeyId>)> cb,
             scoped_refptr<Decryptor> decryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(decryptor->keys_sequence_checker_);
            StatusOr<Encryptor::PublicKeyId> result =
                CreateUnknownErrorStatusOr();
            if (key_info.private_key.size() != kKeySize) {
              result = base::unexpected(Status(
                  error::FAILED_PRECONDITION,
                  base::StrCat(
                      {"Private key size mismatch, expected=",
                       base::NumberToString(kKeySize), " actual=",
                       base::NumberToString(key_info.private_key.size())})));
            } else if (public_key.size() != kKeySize) {
              result = base::unexpected(Status(
                  error::FAILED_PRECONDITION,
                  base::StrCat({"Public key size mismatch, expected=",
                                base::NumberToString(kKeySize), " actual=",
                                base::NumberToString(public_key.size())})));
            } else {
              // Assign a random number to be public key id for testing purposes
              // only (in production it will be retrieved from the server as
              // 'int32').
              const Encryptor::PublicKeyId public_key_id = base::RandGenerator(
                  std::numeric_limits<Encryptor::PublicKeyId>::max());
              if (!decryptor->keys_.emplace(public_key_id, key_info).second) {
                result = base::unexpected(
                    Status(error::ALREADY_EXISTS,
                           base::StrCat({"Public key='", public_key,
                                         "' already recorded"})));
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
              CHECK_EQ(key_info_it->second.private_key.size(),
                       static_cast<size_t>(kKeySize));
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
                        ? StatusOr<std::string>(base::unexpected(Status(
                              error::NOT_FOUND, "Matching key not found")))
                        : key_info_it->second.private_key));
          },
          public_key_id, std::move(cb), base::WrapRefCounted(this)));
}

StatusOr<scoped_refptr<Decryptor>> Decryptor::Create() {
  return base::WrapRefCounted(new Decryptor());
}

}  // namespace test
}  // namespace reporting
