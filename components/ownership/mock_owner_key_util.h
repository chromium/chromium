// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OWNERSHIP_MOCK_OWNER_KEY_UTIL_H_
#define COMPONENTS_OWNERSHIP_MOCK_OWNER_KEY_UTIL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/ownership_export.h"

namespace crypto {
class RSAPrivateKey;
}

namespace ownership {

// Implementation of OwnerKeyUtil which should be used only for
// testing.
class OWNERSHIP_EXPORT MockOwnerKeyUtil : public OwnerKeyUtil {
 public:
  MockOwnerKeyUtil();

  MockOwnerKeyUtil(const MockOwnerKeyUtil&) = delete;
  MockOwnerKeyUtil& operator=(const MockOwnerKeyUtil&) = delete;

  // OwnerKeyUtil implementation:
  scoped_refptr<PublicKey> ImportPublicKey() override;
  crypto::ScopedSECKEYPrivateKey GenerateKeyPair(PK11SlotInfo* slot) override;
  crypto::ScopedSECKEYPrivateKey FindPrivateKeyInSlot(
      const std::vector<uint8_t>& key,
      PK11SlotInfo* slot) override;
  bool IsPublicKeyPresent() override;

  // Clears the public and private keys.
  void Clear();

  // Configures the mock to return the given public key.
  void SetPublicKey(const std::vector<uint8_t>& key);

  // Sets the public key to use from the given private key, but doesn't
  // configure the private key.
  void SetPublicKeyFromPrivateKey(const crypto::RSAPrivateKey& key);

  // Imports the private key into NSS, so it can be found later.
  // Also extracts the public key and sets it for this mock object (equivalent
  // to calling `SetPublicKeyFromPrivateKey`).
  void ImportPrivateKeyAndSetPublicKey(
      std::unique_ptr<crypto::RSAPrivateKey> key);

  // Same as ImportPrivateKeyAndSetPublicKey, but remembers in which slot the
  // key is supposed to be. FindPrivateKeyInSlot will take this into account.
  void ImportPrivateKeyInSlotAndSetPublicKey(
      std::unique_ptr<crypto::RSAPrivateKey> key,
      PK11SlotInfo* slot);

  // Makes next `fail_times` number of calls to OwnerKeyUtil::GenerateKeyPair
  // fail.
  void SimulateGenerateKeyFailure(int fail_times);

 private:
  ~MockOwnerKeyUtil() override;

  void ImportPrivateKeyAndSetPublicKeyImpl(
      std::unique_ptr<crypto::RSAPrivateKey> key,
      PK11SlotInfo* slot);

  int generate_key_fail_times_ = 0;
  std::vector<uint8_t> public_key_;
  std::optional<CK_SLOT_ID> private_key_slot_id_;
  crypto::ScopedSECKEYPrivateKey private_key_;
};

}  // namespace ownership

#endif  // COMPONENTS_OWNERSHIP_MOCK_OWNER_KEY_UTIL_H_
