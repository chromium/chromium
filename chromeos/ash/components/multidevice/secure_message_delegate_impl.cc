// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/dbus/easy_unlock/easy_unlock_client.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::multidevice {

namespace {

// Converts encryption type to a string representation used by EasyUnlock dbus
// client.
std::string EncSchemeToString(securemessage::EncScheme scheme) {
  switch (scheme) {
    case securemessage::AES_256_CBC:
      return easy_unlock::kEncryptionTypeAES256CBC;
    case securemessage::NONE:
      return easy_unlock::kEncryptionTypeNone;
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Converts signature type to a string representation used by EasyUnlock dbus
// client.
std::string SigSchemeToString(securemessage::SigScheme scheme) {
  switch (scheme) {
    case securemessage::ECDSA_P256_SHA256:
      return easy_unlock::kSignatureTypeECDSAP256SHA256;
    case securemessage::HMAC_SHA256:
      return easy_unlock::kSignatureTypeHMACSHA256;
    case securemessage::RSA2048_SHA256:
      // RSA2048_SHA256 is not supported by the daemon.
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace

// static
SecureMessageDelegateImpl::Factory*
    SecureMessageDelegateImpl::Factory::test_factory_instance_ = nullptr;

// static
std::unique_ptr<SecureMessageDelegate>
SecureMessageDelegateImpl::Factory::Create() {
  if (test_factory_instance_)
    return test_factory_instance_->CreateInstance();

  return base::WrapUnique(new SecureMessageDelegateImpl());
}

// static
void SecureMessageDelegateImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_instance_ = test_factory;
}

SecureMessageDelegateImpl::Factory::~Factory() = default;

SecureMessageDelegateImpl::SecureMessageDelegateImpl()
    : dbus_client_(EasyUnlockClient::Get()) {}

SecureMessageDelegateImpl::~SecureMessageDelegateImpl() {}

void SecureMessageDelegateImpl::GenerateKeyPair(
    GenerateKeyPairCallback callback) {
  dbus_client_->GenerateEcP256KeyPair(
      base::BindOnce(&SecureMessageDelegateImpl::OnGenerateKeyPairResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SecureMessageDelegateImpl::DeriveKey(const std::string& private_key,
                                          const std::string& public_key,
                                          DeriveKeyCallback callback) {
  dbus_client_->PerformECDHKeyAgreement(
      private_key, public_key,
      base::BindOnce(&SecureMessageDelegateImpl::OnDeriveKeyResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SecureMessageDelegateImpl::CreateSecureMessage(
    const std::string& payload,
    const std::string& key,
    const CreateOptions& create_options,
    CreateSecureMessageCallback callback) {
  if (create_options.signature_scheme == securemessage::RSA2048_SHA256) {
    PA_LOG(ERROR) << "Unable to create message: RSA2048_SHA256 not supported "
                  << "by the ChromeOS daemon.";
    std::move(callback).Run(std::string());
    return;
  }

  EasyUnlockClient::CreateSecureMessageOptions options;
  options.key.assign(key);

  if (!create_options.associated_data.empty())
    options.associated_data.assign(create_options.associated_data);

  if (!create_options.public_metadata.empty())
    options.public_metadata.assign(create_options.public_metadata);

  if (!create_options.verification_key_id.empty())
    options.verification_key_id.assign(create_options.verification_key_id);

  if (!create_options.decryption_key_id.empty())
    options.decryption_key_id.assign(create_options.decryption_key_id);

  options.encryption_type = EncSchemeToString(create_options.encryption_scheme);
  options.signature_type = SigSchemeToString(create_options.signature_scheme);

  dbus_client_->CreateSecureMessage(
      payload, options,
      base::BindOnce(&SecureMessageDelegateImpl::OnCreateSecureMessageResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SecureMessageDelegateImpl::UnwrapSecureMessage(
    const std::string& serialized_message,
    const std::string& key,
    const UnwrapOptions& unwrap_options,
    UnwrapSecureMessageCallback callback) {
  if (unwrap_options.signature_scheme == securemessage::RSA2048_SHA256) {
    PA_LOG(ERROR) << "Unable to unwrap message: RSA2048_SHA256 not supported "
                  << "by the ChromeOS daemon.";
    std::move(callback).Run(false, std::string(), securemessage::Header());
    return;
  }

  EasyUnlockClient::UnwrapSecureMessageOptions options;
  options.key.assign(key);

  if (!unwrap_options.associated_data.empty())
    options.associated_data.assign(unwrap_options.associated_data);

  options.encryption_type = EncSchemeToString(unwrap_options.encryption_scheme);
  options.signature_type = SigSchemeToString(unwrap_options.signature_scheme);

  dbus_client_->UnwrapSecureMessage(
      serialized_message, options,
      base::BindOnce(&SecureMessageDelegateImpl::OnUnwrapSecureMessageResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SecureMessageDelegateImpl::OnGenerateKeyPairResult(
    GenerateKeyPairCallback callback,
    const std::string& private_key,
    const std::string& public_key) {
  // The SecureMessageDelegate expects the keys in the reverse order returned by
  // the DBus client.
  std::move(callback).Run(public_key, private_key);
}

void SecureMessageDelegateImpl::OnDeriveKeyResult(
    DeriveKeyCallback callback,
    const std::string& derived_key) {
  std::move(callback).Run(derived_key);
}

void SecureMessageDelegateImpl::OnCreateSecureMessageResult(
    CreateSecureMessageCallback callback,
    const std::string& secure_message) {
  std::move(callback).Run(secure_message);
}

void SecureMessageDelegateImpl::OnUnwrapSecureMessageResult(
    UnwrapSecureMessageCallback callback,
    const std::string& unwrap_result) {
  securemessage::HeaderAndBody header_and_body;
  if (!header_and_body.ParseFromString(unwrap_result)) {
    std::move(callback).Run(false, std::string(), securemessage::Header());
  } else {
    std::move(callback).Run(true, header_and_body.body(),
                            header_and_body.header());
  }
}

}  // namespace ash::multidevice
