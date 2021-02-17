// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_ENCRYPTION_ENCRYPTION_MODULE_H_
#define COMPONENTS_REPORTING_ENCRYPTION_ENCRYPTION_MODULE_H_

#include <atomic>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

class EncryptionModule : public base::RefCountedThreadSafe<EncryptionModule> {
 public:
  // Feature to enable/disable encryption.
  // By default encryption is disabled, until server can support decryption.
  static const char kEncryptedReporting[];

  explicit EncryptionModule(base::TimeDelta renew_encryption_key_period =
                                base::TimeDelta::FromDays(1));
  EncryptionModule(const EncryptionModule& other) = delete;
  EncryptionModule& operator=(const EncryptionModule& other) = delete;

  // EncryptRecord will attempt to encrypt the provided |record| and respond
  // with the callback. On success the returned EncryptedRecord will contain
  // the encrypted string and encryption information. EncryptedRecord then can
  // be further updated by the caller.
  virtual void EncryptRecord(
      base::StringPiece record,
      base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) const;

  // Records current public asymmetric key.
  virtual void UpdateAsymmetricKey(
      base::StringPiece new_public_key,
      Encryptor::PublicKeyId new_public_key_id,
      base::OnceCallback<void(Status)> response_cb);

  // Returns `false` if encryption key has not been set yet, and `true`
  // otherwise. The result is lazy: the method may return `false` for some time
  // even after the key has already been set - this is harmless, since resetting
  // or even changing the key is OK at any time.
  bool has_encryption_key() const;

  // Returns `true` if encryption key has not been set yet or it is too old
  // (received more than |renew_encryption_key_period| ago).
  bool need_encryption_key() const;

  // Returns 'true' if |kEncryptedReporting| feature is enabled.
  // To be removed once encryption becomes mandatory.
  static bool is_enabled();

 protected:
  virtual ~EncryptionModule();

 private:
  friend base::RefCountedThreadSafe<EncryptionModule>;

  // Timestamp of the last public asymmetric key update by
  // |UpdateAsymmetricKey|. Initial value base::TimeTicks() indicates key is not
  // set yet.
  std::atomic<base::TimeTicks> last_encryption_key_update_{base::TimeTicks()};

  // Period of encryption key update.
  const base::TimeDelta renew_encryption_key_period_;

  // Encryptor.
  scoped_refptr<Encryptor> encryptor_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_ENCRYPTION_MODULE_H_
