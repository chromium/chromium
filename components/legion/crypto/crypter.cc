// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/crypto/crypter.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_math.h"
#include "crypto/aead.h"

namespace legion {

namespace {

template <size_t N>
std::array<uint8_t, N> Materialize(base::span<const uint8_t, N> span) {
  std::array<uint8_t, N> ret;
  std::copy(span.begin(), span.end(), ret.begin());
  return ret;
}

// Maximum value of a sequence number. Exceeding this causes all operations to
// return an error. This is assumed to be vastly larger than any exchange will
// ever reach.
constexpr uint32_t kMaxSequence = (1 << 24) - 1;

bool ConstructNonce(uint32_t counter, base::span<uint8_t, 12> out_nonce) {
  if (counter > kMaxSequence) {
    return false;
  }

  auto [zeros, counter_span] = out_nonce.split_at<8>();
  std::ranges::fill(zeros, uint8_t{0});
  counter_span.copy_from(base::U32ToBigEndian(counter));
  return true;
}

}  // namespace

Crypter::Crypter(base::span<const uint8_t, 32> read_key,
                 base::span<const uint8_t, 32> write_key)
    : read_key_(Materialize(read_key)), write_key_(Materialize(write_key)) {}

Crypter::~Crypter() = default;

std::optional<std::vector<uint8_t>> Crypter::Encrypt(
    base::span<const uint8_t> plaintext) {
  // Messages will be padded in order to round their length up to a multiple
  // of kPaddingGranularity.
  constexpr size_t kPaddingGranularity = 32;
  static_assert(kPaddingGranularity < 256, "padding too large");
  static_assert(std::has_single_bit(kPaddingGranularity),
                "padding must be a power of two");

  // Padding consists of a some number of zero bytes appended to the message
  // and the final byte in the message is the number of zeros.
  base::CheckedNumeric<size_t> padded_size_checked = plaintext.size();
  padded_size_checked += 1;  // padding-length byte.
  padded_size_checked = (padded_size_checked + kPaddingGranularity - 1) &
                        ~(kPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    NOTREACHED();
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  CHECK_GT(padded_size, plaintext.size());
  const size_t num_zeros = padded_size - plaintext.size() - 1;

  std::vector<uint8_t> padded_message(padded_size, 0);
  std::copy(plaintext.begin(), plaintext.end(), padded_message.begin());
  // The number of added zeros has to fit in a single byte so it has to be
  // less than 256.
  DCHECK_LT(num_zeros, 256u);
  padded_message[padded_message.size() - 1] = static_cast<uint8_t>(num_zeros);

  std::array<uint8_t, 12> nonce;
  if (!ConstructNonce(write_sequence_num_++, nonce)) {
    return std::nullopt;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(write_key_);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  return aes_key.Seal(padded_message, nonce, {});
}

std::optional<std::vector<uint8_t>> Crypter::Decrypt(
    base::span<const uint8_t> ciphertext) {
  std::array<uint8_t, 12> nonce;
  if (!ConstructNonce(read_sequence_num_, nonce)) {
    return std::nullopt;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(read_key_);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  std::optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(ciphertext, nonce, {});

  if (!plaintext) {
    return std::nullopt;
  }
  ++read_sequence_num_;

  if (plaintext->empty()) {
    LOG(ERROR) << "Invalid legion message.";
    return std::nullopt;
  }

  const size_t padding_length = (*plaintext)[plaintext->size() - 1];
  if (padding_length + 1 > plaintext->size()) {
    LOG(ERROR) << "Invalid legion message.";
    return std::nullopt;
  }
  plaintext->resize(plaintext->size() - padding_length - 1);

  return plaintext;
}

}  // namespace legion
