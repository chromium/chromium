// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_TEST_FAKE_CRYPTOGRAPHER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "components/sync/engine/nigori/cryptographer.h"

namespace syncer {

// An implementation of Cryptographer that encrypts data by trivially combining
// the input with the encryption key identifier. Multiple keys can be made
// available to the class via AddEncryptionKey(), and data encrypted with any
// of the available keys can be decrypted. The concept of "key name" is simply
// the string identifier used when adding the key.
class FakeCryptographer : public Cryptographer {
 public:
  static std::unique_ptr<FakeCryptographer> FromSingleDefaultKey(
      const std::string& key_name);

  FakeCryptographer();
  ~FakeCryptographer() override;

  FakeCryptographer(const FakeCryptographer&) = delete;
  FakeCryptographer& operator=(const FakeCryptographer&) = delete;

  // |key_name| is a string able to identify the key consistently. It must not
  // be empty.
  void AddEncryptionKey(const std::string& key_name);
  // |key_name| must have been previously added. Once this is called, |key_name|
  // will be the return value of GetDefaultEncryptionKeyName();
  void SelectDefaultEncryptionKey(const std::string& key_name);
  void ClearDefaultEncryptionKey();

  const CrossUserSharingPublicPrivateKeyPair& GetCrossUserSharingKeyPair(
      uint32_t version) const;

  // Cryptographer implementation.
  bool CanEncrypt() const override;
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const override;
  std::string GetDefaultEncryptionKeyName() const override;
  bool EncryptString(const std::string& decrypted,
                     sync_pb::EncryptedData* encrypted) const override;
  bool DecryptToString(const sync_pb::EncryptedData& encrypted,
                       std::string* decrypted) const override;
  std::optional<std::vector<uint8_t>> AuthEncryptForCrossUserSharing(
      base::span<const uint8_t> plaintext,
      base::span<const uint8_t> recipient_public_key) const override;
  std::optional<std::vector<uint8_t>> AuthDecryptForCrossUserSharing(
      base::span<const uint8_t> encrypted_data,
      base::span<const uint8_t> sender_public_key,
      const uint32_t recipient_key_version) const override;

 private:
  std::set<std::string> known_key_names_;
  // The state with no default key is encoded with an empty string.
  std::string default_key_name_;
  CrossUserSharingPublicPrivateKeyPair cross_user_sharing_key_pair_ =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_CRYPTOGRAPHER_H_
