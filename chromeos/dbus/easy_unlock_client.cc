// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/easy_unlock_client.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Reads array of bytes from a dbus message reader and converts it to string.
std::string PopResponseData(dbus::MessageReader* reader) {
  const uint8_t* bytes = NULL;
  size_t length = 0;
  if (!reader->PopArrayOfBytes(&bytes, &length))
    return "";

  return std::string(reinterpret_cast<const char*>(bytes), length);
}

// Converts string to array of bytes and writes it using dbus meddage writer.
void AppendStringAsByteArray(const std::string& data,
                             dbus::MessageWriter* writer) {
  writer->AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(data.data()),
                             data.length());
}

// The EasyUnlockClient used in production (and returned by
// EasyUnlockClient::Create).
class EasyUnlockClientImpl : public EasyUnlockClient {
 public:
  EasyUnlockClientImpl() : proxy_(nullptr) {}

  ~EasyUnlockClientImpl() override = default;

  // EasyUnlockClient override.
  void GenerateEcP256KeyPair(KeyPairCallback callback) override {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kGenerateEcP256KeyPairMethod);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&EasyUnlockClientImpl::OnKeyPair,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // EasyUnlockClient override.
  void WrapPublicKey(const std::string& key_algorithm,
                     const std::string& public_key,
                     DataCallback callback) override {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kWrapPublicKeyMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(key_algorithm);
    AppendStringAsByteArray(public_key, &writer);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&EasyUnlockClientImpl::OnData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // EasyUnlockClient override.
  void PerformECDHKeyAgreement(const std::string& private_key,
                               const std::string& public_key,
                               DataCallback callback) override {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kPerformECDHKeyAgreementMethod);
    dbus::MessageWriter writer(&method_call);
    // NOTE: DBus expects that data sent as string is UTF-8 encoded. This is
    //     not guaranteed here, so the method uses byte arrays.
    AppendStringAsByteArray(private_key, &writer);
    AppendStringAsByteArray(public_key, &writer);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&EasyUnlockClientImpl::OnData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // EasyUnlockClient override.
  void CreateSecureMessage(const std::string& payload,
                           const CreateSecureMessageOptions& options,
                           DataCallback callback) override {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kCreateSecureMessageMethod);
    dbus::MessageWriter writer(&method_call);
    // NOTE: DBus expects that data sent as string is UTF-8 encoded. This is
    //     not guaranteed here, so the method uses byte arrays.
    AppendStringAsByteArray(payload, &writer);
    AppendStringAsByteArray(options.key, &writer);
    AppendStringAsByteArray(options.associated_data, &writer);
    AppendStringAsByteArray(options.public_metadata, &writer);
    AppendStringAsByteArray(options.verification_key_id, &writer);
    AppendStringAsByteArray(options.decryption_key_id, &writer);
    writer.AppendString(options.encryption_type);
    writer.AppendString(options.signature_type);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&EasyUnlockClientImpl::OnData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // EasyUnlockClient override.
  void UnwrapSecureMessage(const std::string& message,
                           const UnwrapSecureMessageOptions& options,
                           DataCallback callback) override {
    dbus::MethodCall method_call(
        easy_unlock::kEasyUnlockServiceInterface,
        easy_unlock::kUnwrapSecureMessageMethod);
    dbus::MessageWriter writer(&method_call);
    // NOTE: DBus expects that data sent as string is UTF-8 encoded. This is
    //     not guaranteed here, so the method uses byte arrays.
    AppendStringAsByteArray(message, &writer);
    AppendStringAsByteArray(options.key, &writer);
    AppendStringAsByteArray(options.associated_data, &writer);
    writer.AppendString(options.encryption_type);
    writer.AppendString(options.signature_type);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&EasyUnlockClientImpl::OnData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ =
        bus->GetObjectProxy(
            easy_unlock::kEasyUnlockServiceName,
            dbus::ObjectPath(easy_unlock::kEasyUnlockServicePath));
  }

 private:
  void OnData(DataCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::string());
      return;
    }

    dbus::MessageReader reader(response);
    std::move(callback).Run(PopResponseData(&reader));
  }

  void OnKeyPair(KeyPairCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::string(), std::string());
      return;
    }

    dbus::MessageReader reader(response);
    std::string private_key = PopResponseData(&reader);
    std::string public_key = PopResponseData(&reader);

    if (public_key.empty() || private_key.empty()) {
      std::move(callback).Run(std::string(), std::string());
      return;
    }

    std::move(callback).Run(private_key, public_key);
  }

  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EasyUnlockClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockClientImpl);
};

}  // namespace

EasyUnlockClient::CreateSecureMessageOptions::CreateSecureMessageOptions() =
    default;

EasyUnlockClient::CreateSecureMessageOptions::~CreateSecureMessageOptions() =
    default;

EasyUnlockClient::UnwrapSecureMessageOptions::UnwrapSecureMessageOptions() =
    default;

EasyUnlockClient::UnwrapSecureMessageOptions::~UnwrapSecureMessageOptions() =
    default;

EasyUnlockClient::EasyUnlockClient() = default;

EasyUnlockClient::~EasyUnlockClient() = default;

// static
std::unique_ptr<EasyUnlockClient> EasyUnlockClient::Create() {
  return std::make_unique<EasyUnlockClientImpl>();
}

}  // namespace chromeos
