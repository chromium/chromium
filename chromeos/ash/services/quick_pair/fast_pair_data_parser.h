// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DATA_PARSER_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DATA_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

inline constexpr int kEncryptedDataByteSize = 16;
inline constexpr int kAesBlockByteSize = 16;

namespace ash {
namespace quick_pair {

// This class is responsible for parsing the untrusted bytes from a Bluetooth
// device during Fast Pair.
class FastPairDataParser : public mojom::FastPairDataParser {
 public:
  explicit FastPairDataParser(
      mojo::PendingReceiver<mojom::FastPairDataParser> receiver);
  ~FastPairDataParser() override;
  FastPairDataParser(const FastPairDataParser&) = delete;
  FastPairDataParser& operator=(const FastPairDataParser&) = delete;

  // Gets the hex string representation of the device's model ID from the
  // service data.
  void GetHexModelIdFromServiceData(
      const std::vector<uint8_t>& service_data,
      GetHexModelIdFromServiceDataCallback callback) override;

  // Decrypts |encrypted_response_bytes| using |aes_key| and returns a parsed
  // DecryptedResponse instance if possible.
  void ParseDecryptedResponse(
      const std::vector<uint8_t>& aes_key_bytes,
      const std::vector<uint8_t>& encrypted_response_bytes,
      ParseDecryptedResponseCallback callback) override;

  // Decrypts |encrypted_passkey_bytes| using |aes_key| and returns a parsed
  // DecryptedPasskey instance if possible.
  void ParseDecryptedPasskey(
      const std::vector<uint8_t>& aes_key_bytes,
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      ParseDecryptedPasskeyCallback callback) override;

  // Attempts to parse a 'Not Discoverable' advertisement from |service_data|.
  // If the advertisement does not contain information about salt, use the
  // |address| as salt instead.
  void ParseNotDiscoverableAdvertisement(
      const std::vector<uint8_t>& service_data,
      const std::string& address,
      ParseNotDiscoverableAdvertisementCallback callback) override;

  // Attempts to parse MessageStreamMessage instances from |message_bytes| and
  // stores results in array to pass to callback on success.
  void ParseMessageStreamMessages(
      const std::vector<uint8_t>& message_bytes,
      ParseMessageStreamMessagesCallback callback) override;

 private:
  mojom::MessageStreamMessagePtr ParseMessageStreamMessage(
      mojom::MessageGroup message_group,
      uint8_t message_code,
      const base::span<uint8_t>& additional_data);

  mojom::MessageStreamMessagePtr ParseBluetoothEvent(uint8_t message_code);

  mojom::MessageStreamMessagePtr ParseCompanionAppEvent(uint8_t message_code);

  mojom::MessageStreamMessagePtr ParseDeviceInformationEvent(
      uint8_t message_code,
      const base::span<uint8_t>& additional_data);

  mojom::MessageStreamMessagePtr ParseDeviceActionEvent(
      uint8_t message_code,
      const base::span<uint8_t>& additional_data);

  mojom::MessageStreamMessagePtr ParseAcknowledgementEvent(
      uint8_t message_code,
      const base::span<uint8_t>& additional_data);

  mojo::Receiver<mojom::FastPairDataParser> receiver_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_FAST_PAIR_DATA_PARSER_H_
