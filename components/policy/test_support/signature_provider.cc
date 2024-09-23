// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/signature_provider.h"

#include <stdint.h>

#include <cstdint>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// TODO(b:314810831, b:325026413): Update the signatures to match the key used
// for testing defined by PolicyBuilder, or update them to support the
// PublicKeyVerificationData type included in new_public_key_verification_data.
constexpr char kSigningKey1[] =
    "MIIBVQIBADANBgkqhkiG9w0BAQEFAASCAT8wggE7AgEAAkEA2c3KzcPqvnJ5HCk3OZkf1"
    "LMO8Ht4dw4FO2U0EmKvpo0zznj4RwUdmKobH1AFWzwZP4CDY2M67MsukE/1Jnbx1QIDAQ"
    "ABAkBkKcLZa/75hHVz4PR3tZaw34PATlfxEG6RiRIwXlf/FFlfGIZOSxdW/I1A3XRl0/9"
    "nZMuctBSKBrcTRZQWfT/hAiEA9g8xbQbMO6BEH/XCRSsQbPlvj4c9wDtVEzeAzZ/ht9kC"
    "IQDiml+/lXS1emqml711jJcYJNYJzdy1lL/ieKogR59oXQIhAK+Pl4xa1U2VxAWpq7r+R"
    "vH55wdZT03hB4p2h4gvEzXBAiAkw9kvE0eZPiBZoRrrHIFTOH7FnnHlwBmV2+/2RsiVPQ"
    "IhAKqx/4qisivvmoM/xbzUagfoxwsu1A/4mGjhBKiS0BCq";

constexpr char kSigningKey2[] =
    "MIIBVAIBADANBgkqhkiG9w0BAQEFAASCAT4wggE6AgEAAkEAmZhreV04M3knCi6wibr49"
    "oDesHny1G33PKOX9ko8pcxAiu9ZqsKCj7wNW2PGqnLi81fddACwQtYn5xdhCtzB9wIDAQ"
    "ABAkA0z8m0cy8N08xundspoFZWO71WJLgv/peSDBYGI0RzJR1l9Np355EukQUQwRs5XrL"
    "3vRQZy2vDqeiR96epkAhRAiEAzJ4DVI8k3pAl7CGv5icqFkJ02viExIwehhIEXBcB6p0C"
    "IQDAKmzpoRpBEZRQ9xrTvPOi+Ea8Jnd478BU7CI/LFfgowIgMfLIoVWoDGRnvXKju60Hy"
    "xNB70oHLut9cADp64j6QMkCIDrgxN4QbmrhaAAmtiGKE1wrlgCwCIsVamiasSOKAqLhAi"
    "EAo/ItVcFtQPod97qG71CY/O4JzOciuU6AMhprs181vfM=";

constexpr char kTestDomain1Signature1[] =
    "l+sT5mziei/GbmiP7VtRCCfwpZcg7uKbW2OlnK5B/TTELutjEIAMdHduNBwbO44qOn"
    "/5c7YrtkXbBehaaDYFPGI6bGTbDmG9KRxhS+DaB7opgfCQWLi79Gn/jytKLZhRN/VS"
    "y+PEbezqMi3d1/xDxlThwWZDNwnhv9ER/Nu/32ZTjzgtqonSn2CQtwXCIILm4FdV/1"
    "/BdmZG+Ge4i4FTqYtInir5YFe611KXU/AveGhQGBIAXo4qYg1IqbVrvKBSU9dlI6Sl"
    "9TJJLbJ3LGaXuljgFhyMAl3gcy7ftC9MohEmwa+sc7y2mOAgYQ5SSmyAtQwQgAkX9J"
    "3+tfxjmoA/dg==";

constexpr char kTestDomain1Signature2[] =
    "cO0nQjRptkeefKDw5QpJSQDavHABxUvbR9Wvoa235OG9Whw1RFqq2ye6pKnI3ezW6/"
    "7b4ANcpi5a7HV5uF8K7gWyYdxY8NHLeyrbwXxg5j6HAmHmkP1UZcf/dAnWqo7cW8g4"
    "DIQOhC43KkveMYJ2HnelwdXt/7zqkbe8/3Yj4nhjAUeARx86Sb8Nzydwkrvqs5Jw/x"
    "5LG+BODExrXXcGu/ubDlW4ivJFqfNUPQysqBXSMY2XCHPJDx3eECLGVVN/fFAWWgjM"
    "HFObAriAt0b18cc9Nr0mAt4Qq1oDzWcAHCPHE+5dr8Uf46BUrMLJRNRKCY7rrsoIin"
    "9Be9gs3W+Aww==";

constexpr char kTestDomain2Signature1[] =
    "TzBiigZKwBdr6lyP6tUDsw+Q9wYO1Yepyxm0O4JZ4RID32L27sWzC1/hwC51fRcCvP"
    "luEVIW6mH+BFODXMrteUFWfbbG7jgV+Wg+QdzMqgJjxhNKFXPTsZ7/286LAd1vBY/A"
    "nGd8Wog6AhzfrgMbLNsH794GD0xIUwRvXUWFNP8pClj5VPgQnJrIA9aZwW8FNGbteA"
    "HacFB0T/oqP5s7XT4Qvkj14RLmCgTwEM8Vcpqy5teJaF8yN17wniveddoOQGH6s0HC"
    "ocprEccrH5fP/WVAPxCfx4vVYQY5q4CZ4K3f6dTC2FV4IDelM6dugEkvSS02YCzDaO"
    "N+Z7IwElzTKg==";

constexpr char kTestDomain2Signature2[] =
    "mr+9CCYvR0cTvPwlzkxqlpGYy55gY7cPiIkPAPoql51yHK1tkMTOSFru8Dy/nMt+0o"
    "4z7WO60F1wnIBGkQxnTj/DsO6QpCYi7oHqtLmZ2jsLQFlMyvPGUtpJEFvRwjr/TNbh"
    "6RqUtz1LQFuJQ848kBrx7nkte1L8SuPDExgx+Q3LtbNj4SuTdvMUBMvEERXiLuwfFL"
    "BefGjtsqfWETQVlJTCW7xcqOLedIX8UYgEDBpDOZ23A3GzCShuBsIut5m87R5mODht"
    "EUmKNDK1+OMc6SyDpf+r48Wph4Db1bVaKy8fcpSNJOwEgsrmH7/+owKPGcN7I5jYAF"
    "Z2PGxHTQ9JNA==";

constexpr char kTestDomain3Signature1[] =
    "T0wXC5w3GXyovA09pyOLX7ui/NI603UfbZXYyTbHI7xtzCIaHVPH35Nx4zdqVrdsej"
    "ErQ12yVLDDIJokY4Yl+/fj/zrkAPxThI+TNQ+jo0i+al05PuopfpzvCzIXiZBbkbyW"
    "3XfedxXP3IPN2XU2/3vX+ZXUNG6pxeETem64kGezkjkUraqnHw3JVzwJYHhpMcwdLP"
    "PYK6V23BbEHEVBtQZd/ledXacz7gOzm1zGni4e+vxA2roAdJWyhbjU0dTKNNUsZmMv"
    "ryQH9Af1Jw+dqs0RAbhcJXm2i8EUWIgNv6aMn1Z2DzZwKKjXsKgcYSRo8pdYa8RZAo"
    "UExd9roA9a5w==";

constexpr char kTestDomain3Signature2[] =
    "o5MVSo4bRwIJ/aooGyXpRXsEsWPG8fNA2UTG8hgwnLYhNeJCCnLs/vW2vdp0URE8jn"
    "qiG4N8KjbuiGw0rJtO1EygdLfpnMEtqYlFjrOie38sy92l/AwohXj6luYzMWL+FqDu"
    "WQeXasjgyY4s9BOLQVDEnEj3pvqhrk/mXvMwUeXGpbxTNbWAd0C8BTZrGOwU/kIXxo"
    "vAMGg8L+rQaDwBTEnMsMZcvlrIyqSg5v4BxCWuL3Yd2xvUqZEUWRp1aKetsHRnz5hw"
    "H7WK7DzvKepDn06XjPG9lchi448U3HB3PRKtCzfO3nD9YXMKTuqRpKPF8PeK11CWh1"
    "DBvBYwi20vbQ==";

constexpr char kWildCard[] = "*";

std::unique_ptr<crypto::RSAPrivateKey> DecodePrivateKey(
    const char* const encoded) {
  std::string to_decrypt;
  if (!base::Base64Decode(encoded, &to_decrypt)) {
    return nullptr;
  }

  return crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
      base::as_bytes(base::make_span(to_decrypt)));
}

bool ExportPublicKeyAsString(const crypto::RSAPrivateKey& private_key,
                             std::string* public_key) {
  std::vector<uint8_t> public_key_vec;
  if (!private_key.ExportPublicKey(&public_key_vec)) {
    return false;
  }

  public_key->assign(reinterpret_cast<const char*>(public_key_vec.data()),
                     public_key_vec.size());
  return true;
}

std::string Decode64(const std::string& str64) {
  std::string result;
  CHECK(base::Base64Decode(str64, &result));
  return result;
}

void InitSigningKeys(std::vector<SignatureProvider::SigningKey>* signing_keys) {
  signing_keys->push_back(SignatureProvider::SigningKey(
      DecodePrivateKey(kSigningKey1),
      {
          {SignatureProvider::kTestDomain1, Decode64(kTestDomain1Signature1)},
          {SignatureProvider::kTestDomain2, Decode64(kTestDomain2Signature1)},
          {SignatureProvider::kTestDomain3, Decode64(kTestDomain3Signature1)},
      }));

  signing_keys->push_back(SignatureProvider::SigningKey(
      DecodePrivateKey(kSigningKey2),
      {
          {SignatureProvider::kTestDomain1, Decode64(kTestDomain1Signature2)},
          {SignatureProvider::kTestDomain2, Decode64(kTestDomain2Signature2)},
          {SignatureProvider::kTestDomain3, Decode64(kTestDomain3Signature2)},
      }));
}

}  // namespace

constexpr char SignatureProvider::kTestDomain1[];
constexpr char SignatureProvider::kTestDomain2[];
constexpr char SignatureProvider::kTestDomain3[];

SignatureProvider::SigningKey::SigningKey(
    std::unique_ptr<crypto::RSAPrivateKey> private_key,
    const std::map<std::string, std::string>& signatures)
    : private_key_(std::move(private_key)), signatures_(signatures) {
  CHECK(private_key_);
  CHECK(ExportPublicKeyAsString(*private_key_, &public_key_));
}

SignatureProvider::SigningKey::SigningKey(
    SignatureProvider::SigningKey&& signing_key) = default;

SignatureProvider::SigningKey& SignatureProvider::SigningKey::operator=(
    SignatureProvider::SigningKey&& signing_key) = default;

SignatureProvider::SigningKey::~SigningKey() = default;

bool SignatureProvider::SigningKey::GetSignatureForDomain(
    const std::string& domain,
    std::string* signature) const {
  auto domain_signature = signatures_.find(domain);
  if (domain_signature != signatures_.end()) {
    signature->assign(domain_signature->second);
    return true;
  }

  auto wildcard_signature = signatures_.find(kWildCard);
  if (wildcard_signature != signatures_.end()) {
    signature->assign(wildcard_signature->second);
    return true;
  }

  return false;
}

bool SignatureProvider::SigningKey::Sign(
    const std::string& str,
    em::PolicyFetchRequest::SignatureType signature_type,
    std::string* signature) const {
  crypto::SignatureCreator::HashAlgorithm crypto_hash_alg;
  switch (signature_type) {
    case em::PolicyFetchRequest::SHA256_RSA:
      crypto_hash_alg = crypto::SignatureCreator::SHA256;
      break;
    case em::PolicyFetchRequest::NONE:
    case em::PolicyFetchRequest::SHA1_RSA:
      crypto_hash_alg = crypto::SignatureCreator::SHA1;
      break;
  }

  std::unique_ptr<crypto::SignatureCreator> signer =
      crypto::SignatureCreator::Create(private_key_.get(), crypto_hash_alg);

  std::vector<uint8_t> input(str.begin(), str.end());
  std::vector<uint8_t> result;

  if (!signer->Update(input.data(), input.size()) || !signer->Final(&result)) {
    return false;
  }

  signature->assign(std::string(result.begin(), result.end()));

  return true;
}

void SignatureProvider::SetUniversalSigningKeys() {
  std::vector<policy::SignatureProvider::SigningKey> universal_signing_keys;
  universal_signing_keys.push_back(policy::SignatureProvider::SigningKey(
      policy::PolicyBuilder::CreateTestSigningKey(),
      {{kWildCard, policy::PolicyBuilder::GetTestSigningKeySignature()}}));
  set_signing_keys(std::move(universal_signing_keys));
}

void SignatureProvider::SetSigningKeysForChildDomain() {
  std::vector<policy::SignatureProvider::SigningKey> universal_signing_keys;
  universal_signing_keys.push_back(policy::SignatureProvider::SigningKey(
      policy::PolicyBuilder::CreateTestSigningKey(),
      {{kWildCard,
        policy::PolicyBuilder::GetTestSigningKeySignatureForChild()}}));
  set_signing_keys(std::move(universal_signing_keys));
}

bool SignatureProvider::SignVerificationData(const std::string& data,
                                             std::string* signature) const {
  std::unique_ptr<crypto::SignatureCreator> signer =
      crypto::SignatureCreator::Create(verification_key_.get(),
                                       crypto::SignatureCreator::SHA256);

  std::vector<uint8_t> input(data.begin(), data.end());
  std::vector<uint8_t> result;

  if (!signer->Update(input.data(), input.size()) || !signer->Final(&result)) {
    return false;
  }

  signature->assign(std::string(result.begin(), result.end()));

  return true;
}

std::string SignatureProvider::GetVerificationPublicKey() {
  std::string public_key;
  std::vector<uint8_t> public_key_vec;
  CHECK(verification_key_->ExportPublicKey(&public_key_vec));
  public_key.assign(reinterpret_cast<const char*>(public_key_vec.data()),
                    public_key_vec.size());
  return public_key;
}

SignatureProvider::SignatureProvider()
    : verification_key_(crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
          base::span(kVerificationPrivateKey))) {
  InitSigningKeys(&signing_keys_);
}

SignatureProvider::SignatureProvider(SignatureProvider&& signature_provider) =
    default;

SignatureProvider& SignatureProvider::operator=(
    SignatureProvider&& signature_provider) = default;

SignatureProvider::~SignatureProvider() = default;

const SignatureProvider::SigningKey* SignatureProvider::GetKeyByVersion(
    int key_version) const {
  // |key_version| is 1-based.
  if (key_version < 1) {
    return nullptr;
  }
  size_t key_index = static_cast<size_t>(key_version) - 1;
  if (key_index >= signing_keys_.size()) {
    if (!rotate_keys()) {
      return nullptr;
    }
    key_index %= signing_keys_.size();
  }
  return &signing_keys_[key_index];
}

const SignatureProvider::SigningKey* SignatureProvider::GetCurrentKey() const {
  return GetKeyByVersion(current_key_version());
}

}  // namespace policy
