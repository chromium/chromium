// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/securebox.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "crypto/hkdf.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace trusted_vault {

namespace {

const size_t kP256FieldBytes = 32;
const size_t kAES128KeyLength = 16;
const size_t kNonceLength = 12;
const size_t kTagLength = 16;
const size_t kECPrivateKeyLength = 32;
const size_t kECPointLength = 65;
const size_t kVersionLength = 2;
const uint8_t kSecureBoxVersion[] = {0x02, 0};
const uint8_t kHkdfSalt[] = {'S', 'E', 'C', 'U',  'R', 'E',
                             'B', 'O', 'X', 0x02, 0};
const char kHkdfInfoWithPublicKey[] = "P256 HKDF-SHA-256 AES-128-GCM";
const char kHkdfInfoWithoutPublicKey[] = "SHARED HKDF-SHA-256 AES-128-GCM";

// Returns bytes representation of |str| (without trailing \0).
base::span<const uint8_t> StringToBytes(std::string_view str) {
  return base::as_bytes(base::make_span(str));
}

// Concatenates spans in |bytes_spans|.
std::vector<uint8_t> ConcatBytes(
    const std::vector<base::span<const uint8_t>>& bytes_spans) {
  size_t total_size = 0;
  for (const base::span<const uint8_t>& span : bytes_spans) {
    total_size += span.size();
  }

  std::vector<uint8_t> result(total_size);
  auto output_it = result.begin();
  for (const base::span<const uint8_t>& span : bytes_spans) {
    output_it = base::ranges::copy(span, output_it);
  }
  return result;
}

// Creates public EC_KEY from |public_key_bytes|. |public_key_bytes| must be
// a X9.62 formatted NIST P-256 point.
bssl::UniquePtr<EC_KEY> ECPublicKeyFromBytes(
    base::span<const uint8_t> public_key_bytes,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  DCHECK(ec_key);

  bssl::UniquePtr<EC_POINT> point(
      EC_POINT_new(EC_KEY_get0_group(ec_key.get())));
  DCHECK(point);

  if (!EC_POINT_oct2point(EC_KEY_get0_group(ec_key.get()), point.get(),
                          public_key_bytes.data(), kECPointLength,
                          /*ctx=*/nullptr) ||
      !EC_KEY_set_public_key(ec_key.get(), point.get()) ||
      !EC_KEY_check_key(ec_key.get())) {
    // |public_key_bytes| doesn't represent a valid NIST P-256 point.
    return nullptr;
  }

  return ec_key;
}

// Writes |key| point into |output| using X9.62 format.
std::vector<uint8_t> ECPublicKeyToBytes(
    const EC_KEY* key,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  std::vector<uint8_t> result(kECPointLength);
  int export_length = EC_POINT_point2oct(
      EC_KEY_get0_group(key), EC_KEY_get0_public_key(key),
      POINT_CONVERSION_UNCOMPRESSED, result.data(), kECPointLength, nullptr);
  DCHECK_EQ(export_length, static_cast<int>(kECPointLength));
  return result;
}

bssl::UniquePtr<EC_KEY> GenerateECKey(
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  DCHECK(ec_key);

  int generate_key_result = EC_KEY_generate_key(ec_key.get());
  DCHECK(generate_key_result);
  return ec_key;
}

// Computes a 16-byte shared AES-GCM secret. If |private_key| is not null, first
// computes the EC-DH secret. Appends the |shared_secret|, and computes HKDF of
// that. |public_key| and |private_key| might be null, but if either of them is
// not null, other must be not null as well. |shared_secret| may be empty.
std::vector<uint8_t> SecureBoxComputeSecret(
    const EC_KEY* private_key,
    const EC_POINT* public_key,
    base::span<const uint8_t> shared_secret,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  DCHECK_EQ(!!private_key, !!public_key);
  std::vector<uint8_t> dh_secret;
  std::string hkdf_info;
  if (private_key) {
    hkdf_info = kHkdfInfoWithPublicKey;
    dh_secret.resize(kP256FieldBytes);
    int dh_secret_length = ECDH_compute_key(dh_secret.data(), kP256FieldBytes,
                                            public_key, private_key,
                                            /*kdf=*/nullptr);
    CHECK_EQ(dh_secret_length, static_cast<int>(kP256FieldBytes));
  } else {
    hkdf_info = kHkdfInfoWithoutPublicKey;
  }

  std::vector<uint8_t> key_material = ConcatBytes({dh_secret, shared_secret});
  return crypto::HkdfSha256(key_material, kHkdfSalt, StringToBytes(hkdf_info),
                            kAES128KeyLength);
}

// This function implements AES-GCM, using AES-128, a 96-bit nonce, and 128-bit
// tag.
std::vector<uint8_t> SecureBoxAesGcmEncrypt(
    base::span<const uint8_t> secret_key,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> associated_data,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  DCHECK_EQ(secret_key.size(), kAES128KeyLength);
  DCHECK_EQ(nonce.size(), kNonceLength);

  const size_t max_output_length =
      EVP_AEAD_max_overhead(EVP_aead_aes_128_gcm()) + plaintext.size();

  bssl::ScopedEVP_AEAD_CTX ctx;
  size_t output_length;
  std::vector<uint8_t> result(max_output_length);

  int init_result =
      EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_128_gcm(), secret_key.data(),
                        secret_key.size(), kTagLength, nullptr);
  DCHECK(init_result);

  int seal_result = EVP_AEAD_CTX_seal(
      ctx.get(), result.data(), &output_length, max_output_length, nonce.data(),
      nonce.size(), plaintext.data(), plaintext.size(), associated_data.data(),
      associated_data.size());
  CHECK(seal_result);

  DCHECK_LE(output_length, max_output_length);
  result.resize(output_length);
  return result;
}

// Decrypts using AES-GCM.
std::optional<std::vector<uint8_t>> SecureBoxAesGcmDecrypt(
    base::span<const uint8_t> secret_key,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> associated_data,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  const size_t max_output_length = ciphertext.size();

  bssl::ScopedEVP_AEAD_CTX ctx;
  size_t output_length;
  std::vector<uint8_t> result(max_output_length);
  int init_result =
      EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_128_gcm(), secret_key.data(),
                        secret_key.size(), kTagLength, /*impl=*/nullptr);
  DCHECK(init_result);

  if (!EVP_AEAD_CTX_open(ctx.get(), result.data(), &output_length,
                         max_output_length, nonce.data(), nonce.size(),
                         ciphertext.data(), ciphertext.size(),
                         associated_data.data(), associated_data.size())) {
    // |ciphertext| can't be decrypted with given parameters.
    return std::nullopt;
  }

  DCHECK_LE(output_length, max_output_length);
  result.resize(output_length);
  return result;
}

// Creates NIST P-256 EC_KEY given NIST P-256 point multiplier in padded
// big-endian format. Returns nullptr if P-256 key can't be derived using
// |key_bytes| or its format is incorrect.
bssl::UniquePtr<EC_KEY> ImportECPrivateKey(
    base::span<const uint8_t> key_bytes,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  if (key_bytes.size() != kECPrivateKeyLength) {
    return nullptr;
  }

  bssl::UniquePtr<EC_KEY> private_ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  DCHECK(private_ec_key);

  bssl::UniquePtr<BIGNUM> private_key(
      BN_bin2bn(key_bytes.data(), kECPrivateKeyLength, /*ret=*/nullptr));
  if (!private_key ||
      !EC_KEY_set_private_key(private_ec_key.get(), private_key.get())) {
    return nullptr;
  }

  const EC_GROUP* group = EC_KEY_get0_group(private_ec_key.get());
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(group));
  if (!EC_POINT_mul(EC_KEY_get0_group(private_ec_key.get()), point.get(),
                    private_key.get(), /*q=*/nullptr, /*m=*/nullptr,
                    /*ctx=*/nullptr) ||
      !EC_KEY_set_public_key(private_ec_key.get(), point.get()) ||
      !EC_KEY_check_key(private_ec_key.get())) {
    return nullptr;
  }

  return private_ec_key;
}

// |our_key_pair| and |their_public_key| might be null, but if either of them is
// not null, other must be not null as well. |shared_secret|, |header| and
// |payload| may be empty.
std::vector<uint8_t> SecureBoxEncryptImpl(
    const EC_KEY* our_key_pair,
    const EC_POINT* their_public_key,
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  DCHECK_EQ(!!our_key_pair, !!their_public_key);
  std::vector<uint8_t> secret = SecureBoxComputeSecret(
      our_key_pair, their_public_key, shared_secret, err_tracer);

  std::vector<uint8_t> nonce = crypto::RandBytesAsVector(kNonceLength);
  std::vector<uint8_t> ciphertext =
      SecureBoxAesGcmEncrypt(secret, nonce, payload, header, err_tracer);

  std::vector<uint8_t> encoded_our_public_key;
  if (our_key_pair) {
    encoded_our_public_key = ECPublicKeyToBytes(our_key_pair, err_tracer);
  }

  return ConcatBytes(
      {kSecureBoxVersion, encoded_our_public_key, nonce, ciphertext});
}

// |our_private_key| may be null. |shared_secret|, |header| and |payload| may be
// empty. Returns nullopt if decryption failed.
std::optional<std::vector<uint8_t>> SecureBoxDecryptImpl(
    const EC_KEY* our_private_key,
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload) {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  size_t min_payload_size = kVersionLength + kNonceLength;
  if (our_private_key) {
    min_payload_size += kECPointLength;
  }

  if (encrypted_payload.size() < min_payload_size ||
      encrypted_payload[0] != kSecureBoxVersion[0] ||
      encrypted_payload[1] != kSecureBoxVersion[1]) {
    return std::nullopt;
  }

  size_t offset = kVersionLength;
  bssl::UniquePtr<EC_KEY> their_ec_public_key;
  const EC_POINT* their_ec_public_key_point = nullptr;
  if (our_private_key) {
    their_ec_public_key = ECPublicKeyFromBytes(
        encrypted_payload.subspan(offset, kECPointLength), err_tracer);
    if (!their_ec_public_key) {
      return std::nullopt;
    }
    their_ec_public_key_point =
        EC_KEY_get0_public_key(their_ec_public_key.get());
    offset += kECPointLength;
  }

  std::vector<uint8_t> secret_key = SecureBoxComputeSecret(
      our_private_key, their_ec_public_key_point, shared_secret, err_tracer);

  base::span<const uint8_t> nonce =
      encrypted_payload.subspan(offset, kNonceLength);
  offset += kNonceLength;

  base::span<const uint8_t> ciphertext = encrypted_payload.subspan(offset);

  return SecureBoxAesGcmDecrypt(secret_key, nonce, ciphertext, header,
                                err_tracer);
}

}  // namespace

std::vector<uint8_t> SecureBoxSymmetricEncrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload) {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  return SecureBoxEncryptImpl(/*our_key_pair=*/nullptr,
                              /*their_public_key=*/nullptr, shared_secret,
                              header, payload, err_tracer);
}

std::optional<std::vector<uint8_t>> SecureBoxSymmetricDecrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload) {
  return SecureBoxDecryptImpl(/*our_private_key=*/nullptr, shared_secret,
                              header, encrypted_payload);
}

// static
std::unique_ptr<SecureBoxPublicKey> SecureBoxPublicKey::CreateByImport(
    base::span<const uint8_t> key_bytes) {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::UniquePtr<EC_KEY> ec_key = ECPublicKeyFromBytes(key_bytes, err_tracer);
  if (!ec_key) {
    return nullptr;
  }
  return base::WrapUnique(
      new SecureBoxPublicKey(std::move(ec_key), err_tracer));
}

// static
std::unique_ptr<SecureBoxPublicKey> SecureBoxPublicKey::CreateInternal(
    bssl::UniquePtr<EC_KEY> key,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  return base::WrapUnique(new SecureBoxPublicKey(std::move(key), err_tracer));
}

SecureBoxPublicKey::SecureBoxPublicKey(
    bssl::UniquePtr<EC_KEY> key,
    const crypto::OpenSSLErrStackTracer& err_tracer)
    : key_(std::move(key)) {
  DCHECK(EC_KEY_check_key(key_.get()));
  DCHECK_EQ(EC_GROUP_get_curve_name(EC_KEY_get0_group(key_.get())),
            NID_X9_62_prime256v1);
}

SecureBoxPublicKey::~SecureBoxPublicKey() = default;

std::vector<uint8_t> SecureBoxPublicKey::ExportToBytes() const {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  return ECPublicKeyToBytes(key_.get(), err_tracer);
}

std::vector<uint8_t> SecureBoxPublicKey::Encrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload) const {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EC_KEY> our_key_pair = GenerateECKey(err_tracer);
  return SecureBoxEncryptImpl(our_key_pair.get(),
                              EC_KEY_get0_public_key(key_.get()), shared_secret,
                              header, payload, err_tracer);
}

// static
std::unique_ptr<SecureBoxPrivateKey> SecureBoxPrivateKey::CreateByImport(
    base::span<const uint8_t> key_bytes) {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EC_KEY> private_ec_key =
      ImportECPrivateKey(key_bytes, err_tracer);
  if (!private_ec_key) {
    return nullptr;
  }
  return base::WrapUnique(
      new SecureBoxPrivateKey(std::move(private_ec_key), err_tracer));
}

// static
std::unique_ptr<SecureBoxPrivateKey> SecureBoxPrivateKey::CreateInternal(
    bssl::UniquePtr<EC_KEY> key,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  return base::WrapUnique(new SecureBoxPrivateKey(std::move(key), err_tracer));
}

SecureBoxPrivateKey::SecureBoxPrivateKey(
    bssl::UniquePtr<EC_KEY> key,
    const crypto::OpenSSLErrStackTracer& error_tracer)
    : key_(std::move(key)) {
  DCHECK(EC_KEY_get0_private_key(key_.get()));
  DCHECK(EC_KEY_check_key(key_.get()));
  DCHECK_EQ(EC_GROUP_get_curve_name(EC_KEY_get0_group(key_.get())),
            NID_X9_62_prime256v1);
}

SecureBoxPrivateKey::~SecureBoxPrivateKey() = default;

std::vector<uint8_t> SecureBoxPrivateKey::ExportToBytes() const {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::vector<uint8_t> result(kECPrivateKeyLength);
  int bn2bin_result =
      BN_bn2bin_padded(result.data(), kECPrivateKeyLength,
                       /*in=*/EC_KEY_get0_private_key(key_.get()));
  DCHECK(bn2bin_result);
  return result;
}

std::optional<std::vector<uint8_t>> SecureBoxPrivateKey::Decrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload) const {
  return SecureBoxDecryptImpl(key_.get(), shared_secret, header,
                              encrypted_payload);
}

// static
std::unique_ptr<SecureBoxKeyPair> SecureBoxKeyPair::GenerateRandom() {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  return base::WrapUnique(
      new SecureBoxKeyPair(GenerateECKey(err_tracer), err_tracer));
}

// static
std::unique_ptr<SecureBoxKeyPair> SecureBoxKeyPair::CreateByPrivateKeyImport(
    base::span<const uint8_t> private_key_bytes) {
  const crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EC_KEY> private_key =
      ImportECPrivateKey(private_key_bytes, err_tracer);
  if (!private_key) {
    return nullptr;
  }
  return base::WrapUnique(
      new SecureBoxKeyPair(std::move(private_key), err_tracer));
}

SecureBoxKeyPair::SecureBoxKeyPair(
    bssl::UniquePtr<EC_KEY> private_ec_key,
    const crypto::OpenSSLErrStackTracer& err_tracer) {
  DCHECK(private_ec_key);
  bssl::UniquePtr<EC_KEY> public_ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  EC_KEY_set_public_key(public_ec_key.get(),
                        EC_KEY_get0_public_key(private_ec_key.get()));

  private_key_ = SecureBoxPrivateKey::CreateInternal(std::move(private_ec_key),
                                                     err_tracer);
  DCHECK(private_key_);

  public_key_ =
      SecureBoxPublicKey::CreateInternal(std::move(public_ec_key), err_tracer);
  DCHECK(public_key_);
}

SecureBoxKeyPair::~SecureBoxKeyPair() = default;

}  // namespace trusted_vault
