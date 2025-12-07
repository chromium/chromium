// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/fake_magic_arch.h"

#include <algorithm>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "chrome/browser/webauthn/fake_recovery_key_store.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/securebox.h"
#include "crypto/hmac.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace {

const trusted_vault_pb::AsymmetricKeyPair* GetKeyPairWithPublicKey(
    const trusted_vault_pb::Vault& vault,
    std::string_view public_key) {
  for (const auto& key : vault.application_keys()) {
    if (std::string_view(key.asymmetric_key_pair().public_key()) ==
        public_key) {
      return &key.asymmetric_key_pair();
    }
  }
  return nullptr;
}

std::string AsLEBytes(int32_t v) {
  auto bytes = base::I32ToNativeEndian(v);
  return std::string(base::as_string_view(base::span(bytes)));
}

std::array<uint8_t, 32> HashPIN(std::string_view pin,
                                const trusted_vault_pb::Vault& vault) {
  trusted_vault_pb::VaultMetadata metadata;
  CHECK(metadata.ParseFromString(vault.vault_metadata()));
  CHECK_EQ(metadata.hash_type(), trusted_vault_pb::VaultMetadata::SCRYPT);
  const base::span<const uint8_t> salt =
      base::as_byte_span(metadata.hash_salt());
  std::array<uint8_t, 32> hashed;
  CHECK(EVP_PBE_scrypt(pin.data(), pin.size(), salt.data(), salt.size(),
                       metadata.hash_difficulty(), 8, 1,
                       /*max_mem=*/0, hashed.data(), hashed.size()));
  return hashed;
}

std::array<uint8_t, SHA256_DIGEST_LENGTH> SHA256Spans(
    base::span<const uint8_t> a,
    base::span<const uint8_t> b) {
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, a.data(), a.size());
  SHA256_Update(&ctx, b.data(), b.size());
  std::array<uint8_t, SHA256_DIGEST_LENGTH> ret;
  SHA256_Final(ret.data(), &ctx);
  return ret;
}

std::vector<uint8_t> Decrypt(base::span<const uint8_t> key,
                             base::span<const uint8_t> nonce_and_ciphertext) {
  CHECK_GE(nonce_and_ciphertext.size(), 12u);
  const auto [nonce, ciphertext] = nonce_and_ciphertext.split_at<12>();

  EVP_AEAD_CTX ctx;
  CHECK(EVP_AEAD_CTX_init(&ctx, EVP_aead_aes_256_gcm(), key.data(), key.size(),
                          EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr));
  std::vector<uint8_t> result(ciphertext.size());
  size_t result_len;
  CHECK(EVP_AEAD_CTX_open(&ctx, result.data(), &result_len, result.size(),
                          nonce.data(), nonce.size(), ciphertext.data(),
                          ciphertext.size(), nullptr, 0));
  EVP_AEAD_CTX_cleanup(&ctx);
  CHECK_LE(result_len, result.size());
  result.resize(result_len);
  return result;
}

}  // namespace

// static
std::optional<std::vector<uint8_t>> FakeMagicArch::RecoverWithPIN(
    std::string_view pin,
    const FakeSecurityDomainService& security_domain_service,
    const FakeRecoveryKeyStore& recovery_key_store) {
  // Multiple GPM PINs are not supported.
  CHECK_LE(security_domain_service.num_pin_members(), 1u);

  // Find the security domain member.
  const auto member_it = std::ranges::find_if(
      security_domain_service.members(), [](const auto& member) -> bool {
        return member.member_type() ==
               trusted_vault_pb::SecurityDomainMember::
                   MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN;
      });
  if (member_it == security_domain_service.members().end()) {
    return std::nullopt;
  }
  const std::string_view public_key = member_it->public_key();

  // Find the corresponding vault.
  const auto vault_it = std::ranges::find_if(
      recovery_key_store.vaults(), [&public_key](const auto& vault) -> bool {
        return GetKeyPairWithPublicKey(vault, public_key) != nullptr;
      });
  if (vault_it == recovery_key_store.vaults().end()) {
    return std::nullopt;
  }

  const std::array<uint8_t, SHA256_DIGEST_LENGTH> pin_hash =
      HashPIN(pin, *vault_it);
  const auto thm_kf_hash =
      SHA256Spans(base::byte_span_from_cstring("THM_KF_hash"), pin_hash);

  std::string params = "V1 THM_encrypted_recovery_key";
  params += vault_it->vault_parameters().backend_public_key();
  params += vault_it->vault_parameters().counter_id();
  params += AsLEBytes(vault_it->vault_parameters().max_attempts());
  params += vault_it->vault_parameters().vault_handle();

  base::span<const uint8_t> vault_public_key =
      base::as_byte_span(vault_it->vault_parameters().backend_public_key());
  const auto vault_private_key =
      trusted_vault::SecureBoxPrivateKey::CreateByImport(
          recovery_key_store.EndpointPrivateKeyFor(vault_public_key));
  const std::vector<uint8_t> encrypted_recovery_key =
      vault_private_key
          ->Decrypt(thm_kf_hash, base::as_byte_span(params),
                    base::as_byte_span(vault_it->recovery_key()))
          .value();
  const std::vector<uint8_t> recovery_key =
      trusted_vault::SecureBoxSymmetricDecrypt(
          pin_hash,
          base::byte_span_from_cstring("V1 locally_encrypted_recovery_key"),
          encrypted_recovery_key)
          .value();

  const trusted_vault_pb::AsymmetricKeyPair* key_pair =
      GetKeyPairWithPublicKey(*vault_it, public_key);
  const std::vector<uint8_t> wrapping_key =
      trusted_vault::SecureBoxSymmetricDecrypt(
          recovery_key,
          base::byte_span_from_cstring("V1 encrypted_application_key"),
          base::as_byte_span(key_pair->wrapping_key()))
          .value();
  const std::vector<uint8_t> private_key_contents = Decrypt(
      wrapping_key, base::as_byte_span(key_pair->wrapped_private_key()));
  CHECK_GE(private_key_contents.size(), 32u);

  const auto app_private_key =
      trusted_vault::SecureBoxPrivateKey::CreateByImport(
          base::span<const uint8_t>(private_key_contents).first<32>());
  CHECK_EQ(member_it->memberships().size(), 1);
  const trusted_vault_pb::SecurityDomainMember_SecurityDomainMembership&
      membership = member_it->memberships().at(0);
  CHECK_EQ(membership.keys_size(), 1);
  const trusted_vault_pb::SharedMemberKey& shared_member_key =
      membership.keys().at(0);
  const std::vector<uint8_t> security_domain_secret =
      app_private_key
          ->Decrypt(base::span<const uint8_t>(),
                    base::byte_span_from_cstring("V1 shared_key"),
                    base::as_byte_span(shared_member_key.wrapped_key()))
          .value();

  const auto proof = base::span<const uint8_t, crypto::hash::kSha256Size>(
      base::as_byte_span(shared_member_key.member_proof()));
  CHECK(crypto::hmac::VerifySha256(security_domain_secret,
                                   base::as_byte_span(public_key), proof));

  return security_domain_secret;
}
