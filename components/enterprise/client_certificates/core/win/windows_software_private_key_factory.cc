// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/win/windows_software_private_key_factory.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/unexportable_private_key.h"
#include "crypto/unexportable_key.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

scoped_refptr<UnexportablePrivateKey> CreateKey() {
  auto provider = crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
  if (!provider) {
    return nullptr;
  }

  static constexpr std::array<crypto::SignatureVerifier::SignatureAlgorithm, 2>
      kAcceptableAlgorithms = {crypto::SignatureVerifier::ECDSA_SHA256,
                               crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  auto key = provider->GenerateSigningKeySlowly(kAcceptableAlgorithms);

  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<UnexportablePrivateKey>(
      std::move(key), PrivateKeySource::kOsSoftwareKey);
}

scoped_refptr<UnexportablePrivateKey> LoadKeyFromWrapped(
    const std::vector<uint8_t>& wrapped_key) {
  auto provider = crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
  if (!provider) {
    return nullptr;
  }

  auto key = provider->FromWrappedSigningKeySlowly(wrapped_key);
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<UnexportablePrivateKey>(
      std::move(key), PrivateKeySource::kOsSoftwareKey);
}

}  // namespace

// static
std::unique_ptr<WindowsSoftwarePrivateKeyFactory>
WindowsSoftwarePrivateKeyFactory::TryCreate() {
  auto provider = crypto::GetMicrosoftSoftwareUnexportableKeyProvider();

  if (!provider) {
    // OS software keys are not supported.
    return nullptr;
  }

  return base::WrapUnique<WindowsSoftwarePrivateKeyFactory>(
      new WindowsSoftwarePrivateKeyFactory());
}

WindowsSoftwarePrivateKeyFactory::WindowsSoftwarePrivateKeyFactory() = default;

WindowsSoftwarePrivateKeyFactory::~WindowsSoftwarePrivateKeyFactory() = default;

void WindowsSoftwarePrivateKeyFactory::CreatePrivateKey(
    PrivateKeyCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock()},
                                               base::BindOnce(CreateKey),
                                               std::move(callback));
}

void WindowsSoftwarePrivateKeyFactory::LoadPrivateKey(
    const client_certificates_pb::PrivateKey& serialized_private_key,
    PrivateKeyCallback callback) {
  auto private_key_source = ToPrivateKeySource(serialized_private_key.source());
  CHECK(private_key_source.has_value());
  CHECK(private_key_source.value() == PrivateKeySource::kOsSoftwareKey);

  const auto& wrapped_key_str = serialized_private_key.wrapped_key();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          LoadKeyFromWrapped,
          std::vector<uint8_t>(wrapped_key_str.begin(), wrapped_key_str.end())),
      std::move(callback));
}

void WindowsSoftwarePrivateKeyFactory::LoadPrivateKeyFromDict(
    const base::Value::Dict& serialized_private_key,
    PrivateKeyCallback callback) {
  std::optional<int> source = serialized_private_key.FindInt(kKeySource);
  auto* encoded_wrapped_private_key = serialized_private_key.FindString(kKey);

  // Supposed to have been already checked by the parent factory.
  CHECK(source);
  auto source_enum = ToPrivateKeySource(source.value());
  CHECK(source_enum);

  CHECK(encoded_wrapped_private_key);
  std::string decoded_wrapped_private_key;
  if (!base::Base64Decode(*encoded_wrapped_private_key,
                          &decoded_wrapped_private_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  client_certificates_pb::PrivateKey private_key_proto;
  private_key_proto.set_source(ToProtoKeySource(source_enum.value()));
  private_key_proto.set_wrapped_key(std::move(decoded_wrapped_private_key));
  LoadPrivateKey(private_key_proto, std::move(callback));
}

}  // namespace client_certificates
