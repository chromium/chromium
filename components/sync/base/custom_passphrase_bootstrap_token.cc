// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/custom_passphrase_bootstrap_token.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace syncer {

// static
CustomPassphraseBootstrapToken
CustomPassphraseBootstrapToken::FromEncryptedPref(
    const std::string& encrypted_pref,
    const os_crypt_async::Encryptor& encryptor) {
  if (encrypted_pref.empty()) {
    return CustomPassphraseBootstrapToken();
  }

  std::string decoded_key;
  if (!base::Base64Decode(encrypted_pref, &decoded_key)) {
    return CustomPassphraseBootstrapToken();
  }

  std::string decrypted_key;
  bool decryption_result = encryptor.DecryptString(decoded_key, &decrypted_key);
  base::UmaHistogramBoolean("Sync.BootstrapTokenDecryptionResult",
                            decryption_result);
  if (!decryption_result) {
    return CustomPassphraseBootstrapToken();
  }

  sync_pb::NigoriKey key;
  if (!key.ParseFromString(decrypted_key)) {
    return CustomPassphraseBootstrapToken();
  }

  return CustomPassphraseBootstrapToken(std::move(key));
}

// static
CustomPassphraseBootstrapToken CustomPassphraseBootstrapToken::FromProto(
    sync_pb::NigoriKey proto) {
  return CustomPassphraseBootstrapToken(std::move(proto));
}

// static
CustomPassphraseBootstrapToken
CustomPassphraseBootstrapToken::CreateFakeForTesting(int index) {
  sync_pb::NigoriKey proto;
  proto.set_encryption_key("token_" + base::NumberToString(index));
  proto.set_mac_key("mac_" + base::NumberToString(index));
  return CustomPassphraseBootstrapToken(std::move(proto));
}

CustomPassphraseBootstrapToken::CustomPassphraseBootstrapToken() = default;
CustomPassphraseBootstrapToken::~CustomPassphraseBootstrapToken() = default;

CustomPassphraseBootstrapToken::CustomPassphraseBootstrapToken(
    const CustomPassphraseBootstrapToken&) = default;
CustomPassphraseBootstrapToken::CustomPassphraseBootstrapToken(
    CustomPassphraseBootstrapToken&&) = default;
CustomPassphraseBootstrapToken& CustomPassphraseBootstrapToken::operator=(
    const CustomPassphraseBootstrapToken&) = default;
CustomPassphraseBootstrapToken& CustomPassphraseBootstrapToken::operator=(
    CustomPassphraseBootstrapToken&&) = default;

bool CustomPassphraseBootstrapToken::IsEmpty() const {
  return proto_.encryption_key().empty() || proto_.mac_key().empty();
}

std::string CustomPassphraseBootstrapToken::ToEncryptedPref(
    const os_crypt_async::Encryptor& encryptor) const {
  if (IsEmpty()) {
    return std::string();
  }

  const std::string serialized_key = proto_.SerializeAsString();
  if (serialized_key.empty()) {
    return std::string();
  }

  std::string encrypted_key;
  bool encryption_result =
      encryptor.EncryptString(serialized_key, &encrypted_key);
  base::UmaHistogramBoolean("Sync.BootstrapTokenEncryptionResult",
                            encryption_result);
  if (!encryption_result) {
    return std::string();
  }

  return base::Base64Encode(encrypted_key);
}

const sync_pb::NigoriKey& CustomPassphraseBootstrapToken::ToProto() const {
  return proto_;
}

CustomPassphraseBootstrapToken::CustomPassphraseBootstrapToken(
    sync_pb::NigoriKey proto)
    : proto_(std::move(proto)) {}

}  // namespace syncer
