// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/fast_pair_decryption.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_decryption {

constexpr int kMessageTypeIndex = 0;
constexpr int kResponseAddressStartIndex = 1;
constexpr int kResponseSaltStartIndex = 7;
constexpr uint8_t kKeybasedPairingResponseType = 0x01;

// Key-based Pairing Extended Response
constexpr int kResponse2FlagsStartIndex = 1;
constexpr int kResponse2NumberOfAddressesStartIndex = 2;
constexpr int kResponse2AddressStartIndex1 = 3;
constexpr int kResponse2SaltStartIndex1 = 9;
constexpr int kResponse2AddressStartIndex2 = 9;
constexpr int kResponse2SaltStartIndex2 = 15;
constexpr uint8_t kKeybasedPairingExtendedReponseType = 0x02;

constexpr uint8_t kSeekerPasskeyType = 0x02;
constexpr uint8_t kProviderPasskeyType = 0x03;
constexpr int kPasskeySaltStartIndex = 4;

std::array<uint8_t, kBlockByteSize> DecryptBytes(
    const std::array<uint8_t, kBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kBlockByteSize>& encrypted_bytes) {
  AES_KEY aes_key;
  int aes_key_was_set = AES_set_decrypt_key(aes_key_bytes.data(),
                                            aes_key_bytes.size() * 8, &aes_key);
  DCHECK(aes_key_was_set == 0);  // Invalid AES key size.;
  std::array<uint8_t, kBlockByteSize> decrypted_bytes;
  AES_decrypt(encrypted_bytes.data(), decrypted_bytes.data(), &aes_key);
  return decrypted_bytes;
}

// Decrypts the encrypted response
// (https://developers.google.com/nearby/fast-pair/spec#table1.4) and returns
// the parsed decrypted response
// (https://developers.google.com/nearby/fast-pair/spec#table1.3)
std::optional<DecryptedResponse> ParseDecryptedResponse(
    const std::array<uint8_t, kBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kBlockByteSize>& encrypted_response_bytes) {
  std::array<uint8_t, kBlockByteSize> decrypted_response_bytes =
      DecryptBytes(aes_key_bytes, encrypted_response_bytes);

  uint8_t message_type = decrypted_response_bytes[kMessageTypeIndex];

  // Decrypts the Key-based Pairing Extended Response
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      message_type == kKeybasedPairingExtendedReponseType) {
    uint8_t flags = decrypted_response_bytes[kResponse2FlagsStartIndex];
    uint8_t num_addresses =
        decrypted_response_bytes[kResponse2NumberOfAddressesStartIndex];

    std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
    std::copy(decrypted_response_bytes.begin() + kResponse2AddressStartIndex1,
              decrypted_response_bytes.begin() + kResponse2SaltStartIndex1,
              address_bytes.begin());

    std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
    salt.fill(0);

    if (num_addresses == 2) {
      std::array<uint8_t, kDecryptedResponseAddressByteSize>
          secondary_address_bytes;
      std::copy(decrypted_response_bytes.begin() + kResponse2AddressStartIndex2,
                decrypted_response_bytes.begin() + kResponse2SaltStartIndex2,
                secondary_address_bytes.begin());

      std::copy(decrypted_response_bytes.begin() + kResponse2SaltStartIndex2,
                decrypted_response_bytes.end(), salt.begin());

      return DecryptedResponse(
          FastPairMessageType::kKeyBasedPairingExtendedResponse, address_bytes,
          salt, flags, num_addresses, secondary_address_bytes);
    }

    std::copy(decrypted_response_bytes.begin() + kResponse2SaltStartIndex1,
              decrypted_response_bytes.end(), salt.begin());

    return DecryptedResponse(
        FastPairMessageType::kKeyBasedPairingExtendedResponse, address_bytes,
        salt, flags, num_addresses, std::nullopt);
  }

  // If the message type index is not the expected fast pair message type, then
  // this is not a valid fast pair response.
  if (message_type != kKeybasedPairingResponseType) {
    return std::nullopt;
  }

  std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  std::copy(decrypted_response_bytes.begin() + kResponseAddressStartIndex,
            decrypted_response_bytes.begin() + kResponseSaltStartIndex,
            address_bytes.begin());

  std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
  std::copy(decrypted_response_bytes.begin() + kResponseSaltStartIndex,
            decrypted_response_bytes.end(), salt.begin());
  return DecryptedResponse(FastPairMessageType::kKeyBasedPairingResponse,
                           address_bytes, salt);
}

// Decrypts the encrypted passkey
// (https://developers.google.com/nearby/fast-pair/spec#table2.1) and returns
// the parsed decrypted passkey
// (https://developers.google.com/nearby/fast-pair/spec#table2.2)
std::optional<DecryptedPasskey> ParseDecryptedPasskey(
    const std::array<uint8_t, kBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kBlockByteSize>& encrypted_passkey_bytes) {
  std::array<uint8_t, kBlockByteSize> decrypted_passkey_bytes =
      DecryptBytes(aes_key_bytes, encrypted_passkey_bytes);

  FastPairMessageType message_type;
  if (decrypted_passkey_bytes[kMessageTypeIndex] == kSeekerPasskeyType) {
    message_type = FastPairMessageType::kSeekersPasskey;
  } else if (decrypted_passkey_bytes[kMessageTypeIndex] ==
             kProviderPasskeyType) {
    message_type = FastPairMessageType::kProvidersPasskey;
  } else {
    return std::nullopt;
  }

  uint32_t passkey = decrypted_passkey_bytes[3];
  passkey += decrypted_passkey_bytes[2] << 8;
  passkey += decrypted_passkey_bytes[1] << 16;

  std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt;
  std::copy(decrypted_passkey_bytes.begin() + kPasskeySaltStartIndex,
            decrypted_passkey_bytes.end(), salt.begin());
  return DecryptedPasskey(message_type, passkey, salt);
}

}  // namespace fast_pair_decryption
}  // namespace quick_pair
}  // namespace ash
