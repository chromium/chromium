// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/fake_secure_message_delegate.h"

#include <stddef.h>

#include <algorithm>

#include "base/callback.h"
#include "base/hash/md5.h"
#include "base/strings/string_util.h"

namespace chromeos {

namespace multidevice {

namespace {

const char kKeyPrefix[] = "fake_key_";
const char kPrivateKeyPrefix[] = "private_";

// Encrypts the |plaintext| with the |key| using the |encryption_scheme| and
// returns the ciphertext.
std::string Encrypt(const std::string& plaintext,
                    const std::string& key,
                    const securemessage::EncScheme& encryption_scheme) {
  if (encryption_scheme == securemessage::NONE)
    return plaintext;

  // Encrypt with a Vigenere cipher.
  std::string ciphertext;
  ciphertext.resize(plaintext.size());
  for (size_t i = 0; i < plaintext.size(); ++i) {
    unsigned char plaintext_char = plaintext[i];
    unsigned char key_char = key[i % key.size()];
    ciphertext[i] = plaintext_char + key_char;
  }
  return ciphertext;
}

// Decrypts the |ciphertext| with the |key| using the |encryption_scheme| and
// returns the plaintext.
std::string Decrypt(const std::string& ciphertext,
                    const std::string& key,
                    const securemessage::EncScheme& encryption_scheme) {
  if (encryption_scheme == securemessage::NONE)
    return ciphertext;

  // Decrypt with a Vigenere cipher.
  std::string plaintext;
  plaintext.resize(ciphertext.size());
  for (size_t i = 0; i < ciphertext.size(); ++i) {
    unsigned char ciphertext_char = ciphertext[i];
    unsigned char key_char = key[i % key.size()];
    plaintext[i] = ciphertext_char - key_char;
  }
  return plaintext;
}

// Signs the |payload| and |associated_data| with the |key| using the
// |signature_scheme| and returns the signature.
std::string Sign(const std::string& payload,
                 const std::string& associated_data,
                 const std::string& key) {
  return base::MD5String(payload + "|" + associated_data + "|" + key);
}

// Verifies that the |signature| for the the |payload| and |associated_data| is
// valid for the given the |key| and |signature_scheme|.
bool Verify(const std::string& signature,
            const std::string& payload,
            const std::string& associated_data,
            const std::string& key,
            const securemessage::SigScheme& signature_scheme) {
  std::string signing_key;

  if (signature_scheme == securemessage::HMAC_SHA256) {
    signing_key = key;
  } else {
    std::string prefix = kPrivateKeyPrefix;
    bool is_private_key =
        base::StartsWith(key, prefix, base::CompareCase::SENSITIVE);
    signing_key = is_private_key ? key.substr(prefix.size()) : prefix + key;
  }

  std::string expected_signature = Sign(payload, associated_data, signing_key);
  return signature == expected_signature;
}

}  // namespace

FakeSecureMessageDelegate::FakeSecureMessageDelegate()
    : next_public_key_(std::string(kKeyPrefix) + "0") {}

FakeSecureMessageDelegate::~FakeSecureMessageDelegate() = default;

void FakeSecureMessageDelegate::GenerateKeyPair(
    const GenerateKeyPairCallback& callback) {
  std::string public_key = next_public_key_;

  // The private key is simply the public key prepended with "private_".
  std::string private_key(kPrivateKeyPrefix + public_key);

  next_public_key_ = std::string(kKeyPrefix) + base::MD5String(public_key);

  callback.Run(public_key, private_key);
}

void FakeSecureMessageDelegate::DeriveKey(const std::string& private_key,
                                          const std::string& public_key,
                                          const DeriveKeyCallback& callback) {
  // To ensure that the same symmetric key is derived for DeriveKey(private1,
  // public2) and DeriveKey(private2, public1), we remove the prefix from the
  // private key so it is equal to its corresponding public key.
  std::string prefix(kPrivateKeyPrefix);
  std::string normalized_private_key =
      base::StartsWith(private_key, prefix, base::CompareCase::SENSITIVE)
          ? private_key.substr(prefix.size())
          : private_key;

  std::vector<std::string> keys;
  keys.push_back(normalized_private_key);
  keys.push_back(public_key);
  std::sort(keys.begin(), keys.end());
  callback.Run(base::MD5String(keys[0] + "|" + keys[1]));
}

void FakeSecureMessageDelegate::CreateSecureMessage(
    const std::string& payload,
    const std::string& key,
    const CreateOptions& create_options,
    const CreateSecureMessageCallback& callback) {
  securemessage::Header header;
  header.set_signature_scheme(create_options.signature_scheme);
  header.set_encryption_scheme(create_options.encryption_scheme);
  if (!create_options.public_metadata.empty())
    header.set_public_metadata(create_options.public_metadata);
  if (!create_options.verification_key_id.empty())
    header.set_verification_key_id(create_options.verification_key_id);
  if (!create_options.decryption_key_id.empty())
    header.set_decryption_key_id(create_options.decryption_key_id);

  securemessage::HeaderAndBody header_and_body;
  header_and_body.mutable_header()->CopyFrom(header);
  header_and_body.set_body(
      Encrypt(payload, key, create_options.encryption_scheme));
  std::string serialized_header_and_body;
  header_and_body.SerializeToString(&serialized_header_and_body);

  securemessage::SecureMessage secure_message;
  secure_message.set_header_and_body(serialized_header_and_body);
  secure_message.set_signature(
      Sign(payload, create_options.associated_data, key));

  std::string serialized_secure_message;
  secure_message.SerializeToString(&serialized_secure_message);
  callback.Run(serialized_secure_message);
}

void FakeSecureMessageDelegate::UnwrapSecureMessage(
    const std::string& serialized_message,
    const std::string& key,
    const UnwrapOptions& unwrap_options,
    const UnwrapSecureMessageCallback& callback) {
  securemessage::SecureMessage secure_message;
  if (!secure_message.ParseFromString(serialized_message)) {
    LOG(ERROR) << "Failed to parse SecureMessage.";
    callback.Run(false, std::string(), securemessage::Header());
    return;
  }

  securemessage::HeaderAndBody header_and_body;
  if (!header_and_body.ParseFromString(secure_message.header_and_body())) {
    LOG(ERROR) << "Failed to parse secure message HeaderAndBody.";
    callback.Run(false, std::string(), securemessage::Header());
    return;
  }

  const securemessage::Header& header = header_and_body.header();
  std::string payload =
      Decrypt(header_and_body.body(), key, unwrap_options.encryption_scheme);

  bool verified = Verify(secure_message.signature(), payload,
                         unwrap_options.associated_data, key,
                         unwrap_options.signature_scheme);
  if (verified) {
    callback.Run(true, payload, header);
  } else {
    callback.Run(false, std::string(), securemessage::Header());
  }
}

std::string FakeSecureMessageDelegate::GetPrivateKeyForPublicKey(
    const std::string& public_key) {
  return kPrivateKeyPrefix + public_key;
}

std::unique_ptr<multidevice::SecureMessageDelegate>
FakeSecureMessageDelegateFactory::BuildInstance() {
  auto instance = std::make_unique<multidevice::FakeSecureMessageDelegate>();
  instance_ = instance.get();

  return instance;
}

}  // namespace multidevice

}  // namespace chromeos
