// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/nigori.h"

#include <stdint.h>

#include <sstream>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "crypto/aes_cbc.h"
#include "crypto/hmac.h"
#include "crypto/kdf.h"
#include "crypto/random.h"
#include "crypto/subtle_passkey.h"

const size_t kHashSize = 32;
const size_t kDefaultScryptCostParameter = 8192;  // 2^13.

namespace syncer {

namespace {

// NigoriStream simplifies the concatenation operation of the Nigori protocol.
class NigoriStream {
 public:
  // Append the big-endian representation of the length of |value| with 32 bits,
  // followed by |value| itself to the stream.
  NigoriStream& operator<<(const std::string& value) {
    stream_ << base::as_string_view(base::U32ToBigEndian(value.size()));
    stream_ << value;
    return *this;
  }

  // Append the big-endian representation of the length of |type| with 32 bits,
  // followed by the big-endian representation of the value of |type|, with 32
  // bits, to the stream.
  NigoriStream& operator<<(const Nigori::Type type) {
    stream_ << base::as_string_view(base::U32ToBigEndian(sizeof(uint32_t)));
    stream_ << base::as_string_view(base::U32ToBigEndian(type));
    return *this;
  }

  std::string str() { return stream_.str(); }

 private:
  std::ostringstream stream_;
};

const char* GetHistogramSuffixForKeyDerivationMethod(
    KeyDerivationMethod method) {
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return "Pbkdf2";
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return "Scrypt8192";
  }

  NOTREACHED();
}

size_t& GetScryptCostParameter() {
  // Non-const to allow overriding by tests.
  static size_t scrypt_cost_parameter = kDefaultScryptCostParameter;
  return scrypt_cost_parameter;
}

}  // namespace

// static
crypto::SubtlePassKey Nigori::MakeCryptoPassKey() {
  return crypto::SubtlePassKey{};
}

Nigori::Keys::Keys() = default;
Nigori::Keys::~Keys() = default;

void Nigori::SetUseScryptCostParameterForTesting(bool use_low_scrypt_cost) {
  if (use_low_scrypt_cost) {
    GetScryptCostParameter() = 32;
  } else {
    GetScryptCostParameter() = kDefaultScryptCostParameter;
  }
}

void Nigori::Keys::InitByDerivationUsingPbkdf2(const std::string& password) {
  // Previously (<=M70) this value has been recalculated every time based on a
  // constant hostname (hardcoded to "localhost") and username (hardcoded to
  // "dummy") as PBKDF2_HMAC_SHA1(Ns("dummy") + Ns("localhost"), "saltsalt",
  // 1001, 128), where Ns(S) is the NigoriStream representation of S (32-bit
  // big-endian length of S followed by S itself).
  const auto kSalt = std::to_array<uint8_t>({
      // clang-format off
      0xc7, 0xca, 0xfb, 0x23, 0xec, 0x2a, 0x9d, 0x4c,
      0x03, 0x5a, 0x90, 0xae, 0xed, 0x8b, 0xa4, 0x98,
  });  // clang-format on

  // The varying iteration counts here may look odd but this is how Nigori does
  // domain separation when generating subkeys - instead of varying the salt, it
  // varies the iteration count. This has an unfortunate  cryptographic
  // property, namely that
  //
  //   Kenc = Kuser ^ HMAC-SHA1(P, Kuser)
  //   Kmac = Kenc ^ HMAC-SHA1(P, Kenc)
  //
  // but we still do the involved work 3 times over, which is obviously
  // undesirable, but also it's baked into the Nigori spec.

  // Kuser = PBKDF2(P, Suser, Nuser, 16)
  user_key = std::make_optional<std::array<uint8_t, kKeySizeBytes>>();
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(
      crypto::kdf::Pbkdf2HmacSha1Params{.iterations = 1002},
      base::as_byte_span(password), kSalt, user_key.value(),
      Nigori::MakeCryptoPassKey());

  // Kenc = PBKDF2(P, Suser, Nenc, 16)
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(
      crypto::kdf::Pbkdf2HmacSha1Params{.iterations = 1003},
      base::as_byte_span(password), kSalt, encryption_key,
      Nigori::MakeCryptoPassKey());

  // Kmac = PBKDF2(P, Suser, Nmac, 16)
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(
      crypto::kdf::Pbkdf2HmacSha1Params{.iterations = 1004},
      base::as_byte_span(password), kSalt, mac_key,
      Nigori::MakeCryptoPassKey());
}

void Nigori::Keys::InitByDerivationUsingScrypt(const std::string& salt,
                                               const std::string& password) {
  // |user_key| is not used anymore. However, old clients may fail to import a
  // Nigori node without one. We initialize it to all zeroes to prevent a
  // failure on those clients.
  user_key = std::make_optional<std::array<uint8_t, kKeySizeBytes>>();

  // Derive a master key twice as long as the required key size, and split it
  // into two to get the encryption and MAC keys.
  const crypto::kdf::ScryptParams params{
      .cost = GetScryptCostParameter(),
      .block_size = 8,
      .parallelization = 11,
      .max_memory_bytes = 32 * 1024 * 1024,  // 32 MiB
  };

  std::array<uint8_t, kKeySizeBytes * 2> keys;
  crypto::kdf::DeriveKeyScrypt(params, base::as_byte_span(password),
                               base::as_byte_span(salt), keys,
                               MakeCryptoPassKey());

  base::span(encryption_key).copy_from(base::span(keys).first(kKeySizeBytes));
  base::span(mac_key).copy_from(base::span(keys).subspan(kKeySizeBytes));
}

bool Nigori::Keys::InitByImport(const std::string& user_key_str,
                                const std::string& encryption_key_str,
                                const std::string& mac_key_str) {
  if (user_key_str.size() == kKeySizeBytes) {
    user_key = std::make_optional<std::array<uint8_t, kKeySizeBytes>>();
    // |user_key| is not used anymore so we tolerate a failed import.
    base::span(user_key.value()).copy_from(base::as_byte_span(user_key_str));
  }

  if (encryption_key_str.size() != encryption_key.size() ||
      mac_key_str.size() != mac_key.size()) {
    return false;
  }

  base::span(encryption_key).copy_from(base::as_byte_span(encryption_key_str));
  base::span(mac_key).copy_from(base::as_byte_span(mac_key_str));

  return true;
}

Nigori::~Nigori() = default;

// static
std::unique_ptr<Nigori> Nigori::CreateByDerivation(
    const KeyDerivationParams& key_derivation_params,
    const std::string& password) {
  return CreateByDerivationImpl(key_derivation_params, password,
                                base::DefaultTickClock::GetInstance());
}

// static
std::unique_ptr<Nigori> Nigori::CreateByImport(
    const std::string& user_key,
    const std::string& encryption_key,
    const std::string& mac_key) {
  // base::WrapUnique() is used because the constructor is private.
  auto nigori = base::WrapUnique(new Nigori());
  if (!nigori->keys_.InitByImport(user_key, encryption_key, mac_key)) {
    return nullptr;
  }
  return nigori;
}

// Permute[Kenc,Kmac](Nigori::Password || kNigoriKeyName)
std::string Nigori::GetKeyName() const {
  static constexpr char kNigoriKeyName[] = "nigori-key";
  NigoriStream plaintext;
  plaintext << Nigori::Password << kNigoriKeyName;

  const std::array<uint8_t, crypto::aes_cbc::kBlockSize> kIv{};
  auto ciphertext = crypto::aes_cbc::Encrypt(
      keys_.encryption_key, kIv, base::as_byte_span(plaintext.str()));
  auto mac = crypto::hmac::SignSha256(keys_.mac_key, ciphertext);

  std::vector<uint8_t> output;
  std::copy(ciphertext.begin(), ciphertext.end(), std::back_inserter(output));
  std::copy(mac.begin(), mac.end(), std::back_inserter(output));
  return base::Base64Encode(output);
}

// Enc[Kenc,Kmac](value)
std::string Nigori::Encrypt(const std::string& value) const {
  std::array<uint8_t, crypto::aes_cbc::kBlockSize> iv;
  crypto::RandBytes(iv);

  auto ciphertext = crypto::aes_cbc::Encrypt(keys_.encryption_key, iv,
                                             base::as_byte_span(value));
  auto mac = crypto::hmac::SignSha256(keys_.mac_key, ciphertext);

  std::vector<uint8_t> output;
  std::copy(iv.begin(), iv.end(), std::back_inserter(output));
  std::copy(ciphertext.begin(), ciphertext.end(), std::back_inserter(output));
  std::copy(mac.begin(), mac.end(), std::back_inserter(output));
  return base::Base64Encode(output);
}

bool Nigori::Decrypt(const std::string& encrypted, std::string* value) const {
  auto input_buf = base::Base64Decode(encrypted);
  if (!input_buf || input_buf->size() < kIvSize * 2 + kHashSize) {
    return false;
  }

  // The input is:
  // * iv (16 bytes)
  // * ciphertext (multiple of 16 bytes)
  // * hash (32 bytes)
  const auto input = base::make_span(*input_buf);
  const auto iv = input.first<kIvSize>();
  const base::span<const uint8_t> ciphertext =
      input.subspan(kIvSize, input.size() - (kIvSize + kHashSize));
  const auto mac = input.last<kHashSize>();

  if (!crypto::hmac::VerifySha256(keys_.mac_key, ciphertext, mac)) {
    return false;
  }

  auto decrypted =
      crypto::aes_cbc::Decrypt(keys_.encryption_key, iv, ciphertext);
  if (decrypted.has_value()) {
    value->assign(base::as_string_view(*decrypted));
    return true;
  } else {
    return false;
  }
}

void Nigori::ExportKeys(std::string* user_key,
                        std::string* encryption_key,
                        std::string* mac_key) const {
  DCHECK(encryption_key);
  DCHECK(mac_key);
  DCHECK(user_key);

  if (keys_.user_key) {
    user_key->assign(base::as_string_view(*keys_.user_key));
  } else {
    user_key->clear();
  }

  encryption_key->assign(base::as_string_view(keys_.encryption_key));
  mac_key->assign(base::as_string_view(keys_.mac_key));
}

// static
std::string Nigori::GenerateScryptSalt() {
  std::string salt(32u, '\0');
  crypto::RandBytes(base::as_writable_byte_span(salt));
  return salt;
}

std::unique_ptr<Nigori> Nigori::CreateByDerivationForTesting(
    const KeyDerivationParams& key_derivation_params,
    const std::string& password,
    const base::TickClock* tick_clock) {
  return CreateByDerivationImpl(key_derivation_params, password, tick_clock);
}

Nigori::Nigori() = default;

// static
std::unique_ptr<Nigori> Nigori::CreateByDerivationImpl(
    const KeyDerivationParams& key_derivation_params,
    const std::string& password,
    const base::TickClock* tick_clock) {
  auto nigori = base::WrapUnique(new Nigori());
  base::TimeTicks begin_time = tick_clock->NowTicks();
  switch (key_derivation_params.method()) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      nigori->keys_.InitByDerivationUsingPbkdf2(password);
      break;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      nigori->keys_.InitByDerivationUsingScrypt(
          key_derivation_params.scrypt_salt(), password);
      break;
  }

  UmaHistogramTimes(
      base::StringPrintf("Sync.Crypto.NigoriKeyDerivationDuration.%s",
                         GetHistogramSuffixForKeyDerivationMethod(
                             key_derivation_params.method())),
      tick_clock->NowTicks() - begin_time);

  return nigori;
}

}  // namespace syncer
