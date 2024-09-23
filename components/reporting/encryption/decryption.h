// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_ENCRYPTION_DECRYPTION_H_
#define COMPONENTS_REPORTING_ENCRYPTION_DECRYPTION_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {
namespace test {

// Full implementation of Decryptor, intended for use in tests and potentially
// in reporting server (wrapped in a Java class).
//
// Curve25519 decryption of the symmetric key with asymmetric private key.
// ChaCha20_Poly1305 decryption and verification of a record in place with
// symmetric key.
//
// Instantiated by an implementation-specific factory:
//   StatusOr<scoped_refptr<Decryptor>> Create();
class Decryptor : public base::RefCountedThreadSafe<Decryptor> {
 public:
  // Decryption record handle, which is created by |OpenRecord| and can accept
  // pieces of data to be decrypted as one record by calling |AddToRecord|
  // multiple times. Resulting decrypted record is available once |CloseRecord|
  // is called.
  class Handle {
   public:
    Handle(std::string_view shared_secret, scoped_refptr<Decryptor> decryptor);
    Handle(const Handle& other) = delete;
    Handle& operator=(const Handle& other) = delete;
    ~Handle();

    // Adds piece of encrypted data to the record.
    void AddToRecord(std::string_view data,
                     base::OnceCallback<void(Status)> cb);

    // Closes and attempts to decrypt the record. Hands over the decrypted data
    // to be processed by the server (or Status if unsuccessful). Accesses key
    // store to attempt all private keys that are considered to be valid,
    // starting with the one that matches the hash. Self-destructs after the
    // callback.
    void CloseRecord(base::OnceCallback<void(StatusOr<std::string_view>)> cb);

   private:
    // Shared secret based on which symmetric key is produced.
    const std::string shared_secret_;

    // Accumulated data to decrypt.
    std::string record_;

    scoped_refptr<Decryptor> decryptor_;
  };

  // Factory method to instantiate the Decryptor.
  static StatusOr<scoped_refptr<Decryptor>> Create();

  // Factory method creates a new record to collect data and decrypt them with
  // the given encrypted key. Hands the handle raw pointer over to the callback,
  // or error status.
  void OpenRecord(std::string_view encrypted_key,
                  base::OnceCallback<void(StatusOr<Handle*>)> cb);

  // Recreates shared secret from local private key and peer public value and
  // returns it or error status.
  StatusOr<std::string> DecryptSecret(std::string_view public_key,
                                      std::string_view peer_public_value);

  // Records a key pair (stores only private key).
  // Executes on a sequenced thread, returns key id or error with callback.
  void RecordKeyPair(
      std::string_view private_key,
      std::string_view public_key,
      base::OnceCallback<void(StatusOr<Encryptor::PublicKeyId>)> cb);

  // Retrieves private key matching the public key hash.
  // Executes on a sequenced thread, returns with callback.
  void RetrieveMatchingPrivateKey(
      Encryptor::PublicKeyId public_key_id,
      base::OnceCallback<void(StatusOr<std::string>)> cb);

 private:
  friend base::RefCountedThreadSafe<Decryptor>;
  Decryptor();
  ~Decryptor();

  // Map of hash(public_key)->{private key, time stamp}
  // Private key is located by the hash of a public key, sent together with the
  // encrypted record. Keys older than pre-defined threshold are discarded.
  // Time stamp allows to drop outdated keys (not implemented yet).
  struct KeyInfo {
    std::string private_key;
    base::Time time_stamp;
  };
  base::flat_map<Encryptor::PublicKeyId, KeyInfo> keys_;

  // Sequential task runner for all keys_ activities:
  // recording, lookup, purge.
  scoped_refptr<base::SequencedTaskRunner> keys_sequenced_task_runner_;

  SEQUENCE_CHECKER(keys_sequence_checker_);
};

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_DECRYPTION_H_
