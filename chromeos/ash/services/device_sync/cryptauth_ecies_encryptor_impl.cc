// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash {

namespace device_sync {

namespace {

constexpr securemessage::EncScheme kSecureMessageEncryptionScheme =
    securemessage::AES_256_CBC;
constexpr securemessage::SigScheme kSecureMessageSignatureScheme =
    securemessage::HMAC_SHA256;

std::optional<securemessage::Header> ParseHeaderFromSerializedSecureMessage(
    const std::string& serialized_secure_message) {
  securemessage::SecureMessage secure_message;
  if (!secure_message.ParseFromString(serialized_secure_message)) {
    PA_LOG(ERROR) << "Error parsing SecureMessage: "
                  << util::EncodeAsString(serialized_secure_message);
    return std::nullopt;
  }

  securemessage::HeaderAndBody header_and_body;
  if (!header_and_body.ParseFromString(secure_message.header_and_body())) {
    PA_LOG(ERROR) << "Error parsing SecureMessage HeaderAndBody: "
                  << util::EncodeAsString(secure_message.header_and_body());
    return std::nullopt;
  }

  return header_and_body.header();
}

bool VerifyEncryptionAndSignatureSchemes(const securemessage::Header& header) {
  if (header.encryption_scheme() != kSecureMessageEncryptionScheme) {
    PA_LOG(ERROR) << "Unexpected SecureMessage encryption scheme: "
                  << header.encryption_scheme()
                  << ". Expected: " << kSecureMessageEncryptionScheme;
    return false;
  }

  if (header.signature_scheme() != kSecureMessageSignatureScheme) {
    PA_LOG(ERROR) << "Unexpected SecureMessage signature scheme: "
                  << header.signature_scheme()
                  << ". Expected: " << kSecureMessageSignatureScheme;
    return false;
  }

  return true;
}

std::optional<std::string> GetSessionPublicKeyFromSecureMessageHeader(
    const securemessage::Header& header) {
  std::string session_public_key = header.decryption_key_id();
  if (session_public_key.empty()) {
    PA_LOG(ERROR) << "The session public key stored in SecureMessage "
                  << "decryption_key_id is empty.";
    return std::nullopt;
  }

  return session_public_key;
}

}  // namespace

// static
CryptAuthEciesEncryptorImpl::Factory*
    CryptAuthEciesEncryptorImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthEciesEncryptor>
CryptAuthEciesEncryptorImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new CryptAuthEciesEncryptorImpl());
}

// static
void CryptAuthEciesEncryptorImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthEciesEncryptorImpl::Factory::~Factory() = default;

CryptAuthEciesEncryptorImpl::CryptAuthEciesEncryptorImpl()
    : secure_message_delegate_(
          multidevice::SecureMessageDelegateImpl::Factory::Create()) {}

CryptAuthEciesEncryptorImpl::~CryptAuthEciesEncryptorImpl() = default;

void CryptAuthEciesEncryptorImpl::OnSingleOutputFinished(
    const std::string& id,
    const std::optional<std::string>& output) {
  DCHECK_GT(remaining_batch_size_, 0u);
  DCHECK(base::Contains(id_to_input_map_, id));
  DCHECK(!output || !output->empty());

  id_to_output_map_.insert_or_assign(id, output);

  --remaining_batch_size_;
  if (remaining_batch_size_ == 0u) {
    secure_message_delegate_.reset();
    OnAttemptFinished(id_to_output_map_);
  }
}

// --------------------------------  Encryption --------------------------------

void CryptAuthEciesEncryptorImpl::OnBatchEncryptionStarted() {
  remaining_batch_size_ = id_to_input_map_.size();

  secure_message_delegate_->GenerateKeyPair(
      base::BindOnce(&CryptAuthEciesEncryptorImpl::OnSessionKeyPairGenerated,
                     base::Unretained(this)));
}

void CryptAuthEciesEncryptorImpl::OnSessionKeyPairGenerated(
    const std::string& session_public_key,
    const std::string& session_private_key) {
  for (const auto& id_input_pair : id_to_input_map_) {
    secure_message_delegate_->DeriveKey(
        session_private_key, id_input_pair.second.key,
        base::BindOnce(
            &CryptAuthEciesEncryptorImpl::OnDiffieHellmanEncryptionKeyDerived,
            base::Unretained(this), id_input_pair.first, session_public_key));
  }
}

void CryptAuthEciesEncryptorImpl::OnDiffieHellmanEncryptionKeyDerived(
    const std::string& id,
    const std::string& session_public_key,
    const std::string& dh_key) {
  multidevice::SecureMessageDelegate::CreateOptions options;
  options.encryption_scheme = kSecureMessageEncryptionScheme;
  options.signature_scheme = kSecureMessageSignatureScheme;
  options.decryption_key_id = session_public_key;

  secure_message_delegate_->CreateSecureMessage(
      id_to_input_map_[id].payload, dh_key, options,
      base::BindOnce(&CryptAuthEciesEncryptorImpl::OnSecureMessageCreated,
                     base::Unretained(this), id));
}

void CryptAuthEciesEncryptorImpl::OnSecureMessageCreated(
    const std::string& id,
    const std::string& serialized_encrypted_secure_message) {
  if (serialized_encrypted_secure_message.empty()) {
    PA_LOG(ERROR) << "Error creating SecureMessage. Input ID: " << id;
    OnSingleOutputFinished(id, std::nullopt /* output */);
    return;
  }

  OnSingleOutputFinished(id, serialized_encrypted_secure_message);
}

// --------------------------------  Decryption --------------------------------

void CryptAuthEciesEncryptorImpl::OnBatchDecryptionStarted() {
  remaining_batch_size_ = id_to_input_map_.size();

  for (const auto& id_input_pair : id_to_input_map_) {
    std::optional<securemessage::Header> header =
        ParseHeaderFromSerializedSecureMessage(id_input_pair.second.payload);
    if (!header) {
      OnSingleOutputFinished(id_input_pair.first, std::nullopt /* output */);
      continue;
    }

    if (!VerifyEncryptionAndSignatureSchemes(*header)) {
      OnSingleOutputFinished(id_input_pair.first, std::nullopt /* output */);
      continue;
    }

    std::optional<std::string> session_public_key =
        GetSessionPublicKeyFromSecureMessageHeader(*header);
    if (!session_public_key) {
      OnSingleOutputFinished(id_input_pair.first, std::nullopt /* output */);
      continue;
    }

    secure_message_delegate_->DeriveKey(
        id_input_pair.second.key, *session_public_key,
        base::BindOnce(
            &CryptAuthEciesEncryptorImpl::OnDiffieHellmanDecryptionKeyDerived,
            base::Unretained(this), id_input_pair.first,
            id_input_pair.second.payload));
  }
}

void CryptAuthEciesEncryptorImpl::OnDiffieHellmanDecryptionKeyDerived(
    const std::string& id,
    const std::string& serialized_encrypted_secure_message,
    const std::string& dh_key) {
  multidevice::SecureMessageDelegate::UnwrapOptions options;
  options.encryption_scheme = kSecureMessageEncryptionScheme;
  options.signature_scheme = kSecureMessageSignatureScheme;

  secure_message_delegate_->UnwrapSecureMessage(
      serialized_encrypted_secure_message, dh_key, options,
      base::BindOnce(&CryptAuthEciesEncryptorImpl::OnSecureMessageUnwrapped,
                     base::Unretained(this), id));
}

void CryptAuthEciesEncryptorImpl::OnSecureMessageUnwrapped(
    const std::string& id,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified || payload.empty()) {
    PA_LOG(ERROR) << "Error verifying and decrypting SecureMessage. Input ID: "
                  << id;
    OnSingleOutputFinished(id, std::nullopt /* output */);
    return;
  }

  OnSingleOutputFinished(id, payload);
}

}  // namespace device_sync

}  // namespace ash
