// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/crypto/agile_symmetric_key.h"

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/sync/model/crypto/nigori.h"
#include "crypto/aead.h"
#include "crypto/random.h"

namespace syncer {

namespace {

constexpr crypto::aead::Algorithm kDefaultAgileSymmetricKeyAlgorithm =
    crypto::aead::AES_256_GCM;

}  // namespace

class AgileSymmetricKey::AeadCipher : public AgileSymmetricKey::Cipher {
 public:
  static std::unique_ptr<AeadCipher> CreateFromAlgorithmAndKeyMaterial(
      crypto::aead::Algorithm alg,
      std::vector<uint8_t> key) {
    if (key.size() != crypto::aead::KeySizeFor(alg)) {
      return nullptr;
    }
    return base::WrapUnique(new AeadCipher(alg, std::move(key)));
  }

  ~AeadCipher() override = default;

  std::vector<uint8_t> Encrypt(
      base::span<const uint8_t> plaintext) const override {
    const crypto::Aead aead(alg_, key_);

    const size_t nonce_size = crypto::aead::NonceSizeFor(alg_);
    std::vector<uint8_t> nonce = crypto::RandBytesAsVector(nonce_size);

    const std::vector<uint8_t> ciphertext =
        aead.Seal(plaintext, nonce, /*additional_data=*/{});

    std::vector<uint8_t> output(nonce.size() + ciphertext.size());
    base::SpanWriter<uint8_t> writer{base::span(output)};
    writer.Write(nonce);
    writer.Write(ciphertext);

    return output;
  }

  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext) const override {
    base::SpanReader<const uint8_t> reader{ciphertext};

    const size_t nonce_size = crypto::aead::NonceSizeFor(alg_);

    const auto nonce = reader.Read(nonce_size);
    if (!nonce) {
      return std::nullopt;
    }

    const base::span<const uint8_t> raw_ciphertext = reader.remaining_span();

    const crypto::Aead aead(alg_, key_);
    return aead.Open(raw_ciphertext, *nonce, /*additional_data=*/{});
  }

  sync_pb::AgileSymmetricKey ToProto() const override {
    sync_pb::AgileSymmetricKey proto;
    switch (alg_) {
      case crypto::aead::AES_256_GCM: {
        auto* aes_proto = proto.mutable_aes_256_gcm();
        aes_proto->set_key(key_.data(), key_.size());
        break;
      }
      case crypto::aead::CHACHA20_POLY1305: {
        auto* chacha_proto = proto.mutable_chacha20_poly1305();
        chacha_proto->set_key(key_.data(), key_.size());
        break;
      }
      case crypto::aead::AES_128_CTR_HMAC_SHA256:
      case crypto::aead::AES_128_GCM:
      case crypto::aead::AES_256_GCM_SIV:
        NOTREACHED();
    }
    return proto;
  }

  std::string GetLegacyNigoriKeyName() const override { return std::string(); }

 private:
  AeadCipher(crypto::aead::Algorithm alg, std::vector<uint8_t> key)
      : alg_(alg), key_(std::move(key)) {}

  const crypto::aead::Algorithm alg_;
  const std::vector<uint8_t> key_;
};

class AgileSymmetricKey::LegacyNigoriCipher : public AgileSymmetricKey::Cipher {
 public:
  static std::unique_ptr<LegacyNigoriCipher> Create(
      std::unique_ptr<Nigori> nigori) {
    if (!nigori) {
      return nullptr;
    }
    return base::WrapUnique(new LegacyNigoriCipher(std::move(nigori)));
  }

  ~LegacyNigoriCipher() override = default;

  std::vector<uint8_t> Encrypt(
      base::span<const uint8_t> plaintext) const override {
    return nigori_->EncryptToBytes(plaintext);
  }

  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext) const override {
    return nigori_->DecryptFromBytes(ciphertext);
  }

  sync_pb::AgileSymmetricKey ToProto() const override {
    sync_pb::AgileSymmetricKey proto;
    auto* legacy_proto = proto.mutable_legacy_nigori();

    std::string user_key, encryption_key, mac_key;
    nigori_->ExportKeys(&user_key, &encryption_key, &mac_key);

    legacy_proto->set_deprecated_user_key(user_key);
    legacy_proto->set_encryption_key(encryption_key);
    legacy_proto->set_mac_key(mac_key);
    return proto;
  }

  std::string GetLegacyNigoriKeyName() const override {
    return nigori_->GetKeyName();
  }

 private:
  explicit LegacyNigoriCipher(std::unique_ptr<Nigori> nigori)
      : nigori_(std::move(nigori)) {}

  const std::unique_ptr<Nigori> nigori_;
};

// static
std::unique_ptr<AgileSymmetricKey> AgileSymmetricKey::CreateRandom() {
  const crypto::aead::Algorithm aead_alg = kDefaultAgileSymmetricKeyAlgorithm;
  std::vector<uint8_t> key =
      crypto::RandBytesAsVector(crypto::aead::KeySizeFor(aead_alg));

  return CreateFromCipherIfNotNull(
      AeadCipher::CreateFromAlgorithmAndKeyMaterial(aead_alg, std::move(key)));
}

// static
std::unique_ptr<AgileSymmetricKey> AgileSymmetricKey::FromLegacyNigoriProto(
    const sync_pb::NigoriKey& proto) {
  return CreateFromCipherIfNotNull(LegacyNigoriCipher::Create(
      Nigori::CreateByImport(NigoriPassKey(), proto.deprecated_user_key(),
                             proto.encryption_key(), proto.mac_key())));
}

// static
std::unique_ptr<AgileSymmetricKey> AgileSymmetricKey::FromProto(
    const sync_pb::AgileSymmetricKey& proto) {
  switch (proto.key_type_case()) {
    case sync_pb::AgileSymmetricKey::KEY_TYPE_NOT_SET:
      return nullptr;
    case sync_pb::AgileSymmetricKey::kAes256Gcm: {
      const auto& aes_proto = proto.aes_256_gcm();
      return CreateFromCipherIfNotNull(
          AeadCipher::CreateFromAlgorithmAndKeyMaterial(
              crypto::aead::AES_256_GCM,
              std::vector<uint8_t>(aes_proto.key().begin(),
                                   aes_proto.key().end())));
    }
    case sync_pb::AgileSymmetricKey::kChacha20Poly1305: {
      const auto& chacha_proto = proto.chacha20_poly1305();
      return CreateFromCipherIfNotNull(
          AeadCipher::CreateFromAlgorithmAndKeyMaterial(
              crypto::aead::CHACHA20_POLY1305,
              std::vector<uint8_t>(chacha_proto.key().begin(),
                                   chacha_proto.key().end())));
    }
    case sync_pb::AgileSymmetricKey::kLegacyNigori: {
      return FromLegacyNigoriProto(proto.legacy_nigori());
    }
  }
  NOTREACHED();
}

AgileSymmetricKey::~AgileSymmetricKey() = default;

std::vector<uint8_t> AgileSymmetricKey::Encrypt(
    base::span<const uint8_t> plaintext) const {
  return cipher_->Encrypt(plaintext);
}

std::optional<std::vector<uint8_t>> AgileSymmetricKey::Decrypt(
    base::span<const uint8_t> ciphertext) const {
  return cipher_->Decrypt(ciphertext);
}

sync_pb::AgileSymmetricKey AgileSymmetricKey::ToProto() const {
  return cipher_->ToProto();
}

std::string AgileSymmetricKey::GetLegacyNigoriKeyName() const {
  return cipher_->GetLegacyNigoriKeyName();
}

AgileSymmetricKey::AgileSymmetricKey(std::unique_ptr<Cipher> cipher)
    : cipher_(std::move(cipher)) {
  CHECK(cipher_);
}

// static
std::unique_ptr<AgileSymmetricKey> AgileSymmetricKey::CreateFromCipherIfNotNull(
    std::unique_ptr<Cipher> cipher) {
  if (!cipher) {
    return nullptr;
  }
  return base::WrapUnique(new AgileSymmetricKey(std::move(cipher)));
}

}  // namespace syncer
