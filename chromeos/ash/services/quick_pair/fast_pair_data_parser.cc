// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "base/base64.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/quick_pair/fast_pair_decryption.h"
#include "chromeos/ash/services/quick_pair/public/cpp/battery_notification.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace {

constexpr int kHeaderIndex = 0;
constexpr int kFieldTypeBitmask = 0b00001111;
constexpr int kFieldLengthBitmask = 0b11110000;
constexpr int kHeaderLength = 1;
constexpr int kFieldLengthOffset = 4;
constexpr int kFieldTypeAccountKeyFilter = 0;
constexpr int kFieldTypeAccountKeyFilterSalt = 1;
constexpr int kFieldTypeAccountKeyFilterNoNotification = 2;
constexpr int kFieldTypeBattery = 3;
constexpr int kFieldTypeBatteryNoNotification = 4;
constexpr int kBluetoothEvent = 0x01;
constexpr int kEnableSilenceModeCode = 0x01;
constexpr int kDisableSilenceModeCode = 0x02;
constexpr int kCompanionAppEvent = 0x02;
constexpr int kLogBufferFullCode = 0x01;
constexpr int kDeviceInformationEvent = 0x03;
constexpr int kModelIdCode = 0x01;
constexpr int kBleAddressUpdatedCode = 0x02;
constexpr int kBatteryUpdatedCode = 0x03;
constexpr int kRemainingBatteryTimeCode = 0x04;
constexpr int kActiveComponentsResponseCode = 0x06;
constexpr int kPlatformTypeCode = 0x08;
constexpr int kAndroidPlatform = 0x01;
constexpr int kDeviceActionEvent = 0x04;
constexpr int kRingCode = 0x01;
constexpr int kAcknowledgementEvent = 0xFF;
constexpr int kAckCode = 0x01;
constexpr int kNakCode = 0x02;
constexpr int kNotSupportedNak = 0x00;
constexpr int kDeviceBusyNak = 0x01;
constexpr int kNotAllowedDueToCurrentStateNak = 0x02;
constexpr uint8_t kBatteryChargeBitmask = 0b10000000;
constexpr uint8_t kBatteryPercentBitmask = 0b01111111;
constexpr int kMinMessageByteCount = 4;
constexpr int kAddressByteSize = 6;

bool ValidateInputSizes(const std::vector<uint8_t>& aes_key_bytes,
                        const std::vector<uint8_t>& encrypted_bytes) {
  if (aes_key_bytes.size() != kAesBlockByteSize) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": AES key should have size = " << kAesBlockByteSize
        << ", actual =  " << aes_key_bytes.size();
    return false;
  }

  if (encrypted_bytes.size() != kEncryptedDataByteSize) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Encrypted bytes should have size = " << kEncryptedDataByteSize
        << ", actual =  " << encrypted_bytes.size();
    return false;
  }

  return true;
}

void ConvertVectorsToArrays(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_bytes,
    std::array<uint8_t, kAesBlockByteSize>& out_aes_key_bytes,
    std::array<uint8_t, kEncryptedDataByteSize>& out_encrypted_bytes) {
  base::ranges::copy(aes_key_bytes, out_aes_key_bytes.begin());
  base::ranges::copy(encrypted_bytes, out_encrypted_bytes.begin());
}

int GetBatteryPercentange(uint8_t battery_byte) {
  int battery_percent = battery_byte & kBatteryPercentBitmask;
  if (battery_percent < 0 || battery_percent > 100)
    return -1;

  return battery_percent;
}

bool IsBatteryCharging(uint8_t battery_byte) {
  return (battery_byte & kBatteryChargeBitmask) != 0;
}

}  // namespace

namespace ash {
namespace quick_pair {

std::optional<mojom::MessageGroup> MessageGroupFromByte(uint8_t message_group) {
  switch (message_group) {
    case kBluetoothEvent:
      return mojom::MessageGroup::kBluetoothEvent;
    case kCompanionAppEvent:
      return mojom::MessageGroup::kCompanionAppEvent;
    case kDeviceInformationEvent:
      return mojom::MessageGroup::kDeviceInformationEvent;
    case kDeviceActionEvent:
      return mojom::MessageGroup::kDeviceActionEvent;
    case kAcknowledgementEvent:
      return mojom::MessageGroup::kAcknowledgementEvent;
    default:
      return std::nullopt;
  }
}

std::optional<mojom::Acknowledgement> NakReasonFromByte(uint8_t nak_reason) {
  switch (nak_reason) {
    case kNotSupportedNak:
      return mojom::Acknowledgement::kNotSupportedNak;
    case kDeviceBusyNak:
      return mojom::Acknowledgement::kDeviceBusyNak;
    case kNotAllowedDueToCurrentStateNak:
      return mojom::Acknowledgement::kNotAllowedDueToCurrentStateNak;
    default:
      return std::nullopt;
  }
}

mojom::BatteryInfoPtr CreateBatteryInfo(uint8_t battery_byte) {
  mojom::BatteryInfoPtr battery_info = mojom::BatteryInfo::New();
  battery_info->is_charging = IsBatteryCharging(battery_byte);
  battery_info->percentage = GetBatteryPercentange(battery_byte);
  return battery_info;
}

FastPairDataParser::FastPairDataParser(
    mojo::PendingReceiver<mojom::FastPairDataParser> receiver)
    : receiver_(this, std::move(receiver)) {}

FastPairDataParser::~FastPairDataParser() = default;

void FastPairDataParser::GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback) {
  std::move(callback).Run(
      fast_pair_decoder::HasModelId(&service_data)
          ? fast_pair_decoder::GetHexModelIdFromServiceData(&service_data)
          : std::nullopt);
}

void FastPairDataParser::ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptedResponseCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_response_bytes)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_response_bytes, key, bytes);

  std::move(callback).Run(
      fast_pair_decryption::ParseDecryptedResponse(key, bytes));
}

void FastPairDataParser::ParseDecryptedPasskey(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    ParseDecryptedPasskeyCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_passkey_bytes)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_passkey_bytes, key, bytes);

  std::move(callback).Run(
      fast_pair_decryption::ParseDecryptedPasskey(key, bytes));
}

void CopyFieldBytes(
    const std::vector<uint8_t>& service_data,
    base::flat_map<size_t, std::pair<size_t, size_t>>& field_indices,
    size_t key,
    std::vector<uint8_t>* out) {
  DCHECK(field_indices.contains(key));

  auto indices = field_indices[key];
  for (size_t i = indices.first; i < indices.second; i++) {
    out->push_back(service_data[i]);
  }
}

void FastPairDataParser::ParseNotDiscoverableAdvertisement(
    const std::vector<uint8_t>& service_data,
    const std::string& address,
    ParseNotDiscoverableAdvertisementCallback callback) {
  if (service_data.empty() ||
      fast_pair_decoder::GetVersion(&service_data) != 0) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::flat_map<size_t, std::pair<size_t, size_t>> field_indices;
  size_t headerIndex = kHeaderIndex + kHeaderLength +
                       fast_pair_decoder::GetIdLength(&service_data);

  while (headerIndex < service_data.size()) {
    size_t type = service_data[headerIndex] & kFieldTypeBitmask;
    size_t length =
        (service_data[headerIndex] & kFieldLengthBitmask) >> kFieldLengthOffset;
    size_t index = headerIndex + kHeaderLength;
    size_t end = index + length;

    if (end <= service_data.size()) {
      field_indices[type] = std::make_pair(index, end);
    }

    headerIndex = end;
  }

  std::vector<uint8_t> account_key_filter_bytes;
  bool show_ui = false;
  if (field_indices.contains(kFieldTypeAccountKeyFilter)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeAccountKeyFilter,
                   &account_key_filter_bytes);
    show_ui = true;
  } else if (field_indices.contains(kFieldTypeAccountKeyFilterNoNotification)) {
    CopyFieldBytes(service_data, field_indices,
                   kFieldTypeAccountKeyFilterNoNotification,
                   &account_key_filter_bytes);
    show_ui = false;
  }

  std::vector<uint8_t> salt_bytes;
  if (field_indices.contains(kFieldTypeAccountKeyFilterSalt)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeAccountKeyFilterSalt,
                   &salt_bytes);
  }

  std::vector<uint8_t> battery_bytes;
  bool show_ui_for_battery = false;
  if (field_indices.contains(kFieldTypeBattery)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeBattery,
                   &battery_bytes);
    show_ui_for_battery = true;
  } else if (field_indices.contains(kFieldTypeBatteryNoNotification)) {
    CopyFieldBytes(service_data, field_indices, kFieldTypeBatteryNoNotification,
                   &battery_bytes);
    show_ui_for_battery = false;
  }

  if (account_key_filter_bytes.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // The salt byte requirements need to stay aligned with the Fast Pair Spec:
  // https://developers.devsite.corp.google.com/nearby/fast-pair/specifications/service/provider#AccountKeyFilter
  if (salt_bytes.size() > 2) {
    CD_LOG(WARNING, Feature::FP)
        << " Parsed a salt field larger than two bytes: " << salt_bytes.size();
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (salt_bytes.empty()) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": missing salt field from device. Using device address instead. ";
    std::array<uint8_t, kAddressByteSize> address_bytes;
    device::ParseBluetoothAddress(address, address_bytes);
    salt_bytes = std::vector(address_bytes.begin(), address_bytes.end());
  }

  std::move(callback).Run(NotDiscoverableAdvertisement(
      std::move(account_key_filter_bytes), show_ui, std::move(salt_bytes),
      BatteryNotification::FromBytes(battery_bytes, show_ui_for_battery)));
}

// https://developers.google.com/nearby/fast-pair/spec#MessageStream
void FastPairDataParser::ParseMessageStreamMessages(
    const std::vector<uint8_t>& message_bytes,
    ParseMessageStreamMessagesCallback callback) {
  std::vector<mojom::MessageStreamMessagePtr> parsed_messages;

  // The minimum mojom::MessageStreamMessage size is four bytes based on the
  // Fast Pair message stream format found here:
  // https://developers.google.com/nearby/fast-pair/spec#MessageStream
  if (message_bytes.size() < kMinMessageByteCount) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Not enough bytes to parse a MessageStreamMessage. "
           "Needed 4, received "
        << message_bytes.size() << ".";
    std::move(callback).Run(std::move(parsed_messages));
    return;
  }

  base::circular_deque<uint8_t> remaining_bytes(base::from_range,
                                                message_bytes);
  while (remaining_bytes.size() >= kMinMessageByteCount) {
    uint8_t message_group_byte = remaining_bytes.front();
    remaining_bytes.pop_front();
    std::optional<mojom::MessageGroup> message_group =
        MessageGroupFromByte(message_group_byte);

    uint8_t message_code = remaining_bytes.front();
    remaining_bytes.pop_front();

    uint16_t byte_to_shift{remaining_bytes.front()};
    remaining_bytes.pop_front();
    uint16_t additional_data_length{remaining_bytes.front()};
    remaining_bytes.pop_front();
    byte_to_shift = byte_to_shift << 8;
    additional_data_length |= byte_to_shift;

    // Only initialize the additional data with the bytes if there is a size for
    // it in the message bytes. Additional data starts at the fourth byte of
    // the message. We want to verify if additional data exists in the
    // message bytes, if not, the data is not trusted and we will return a
    // null message.
    std::vector<uint8_t> additional_data(additional_data_length, 0);
    if (remaining_bytes.size() < additional_data_length)
      break;

    for (int i = 0; i < additional_data_length; ++i) {
      additional_data[i] = remaining_bytes.front();
      remaining_bytes.pop_front();
    }

    // If we have an unknown message group, do not process the Message Stream
    // message. The data was already removed above corresponding to this
    // message, and we can continue to attempt to parse the next message.
    if (!message_group.has_value()) {
      CD_LOG(WARNING, Feature::FP)
          << __func__ << ": Unknown message group. Received 0x" << std::hex
          << message_group_byte << ".";
      continue;
    }

    mojom::MessageStreamMessagePtr message = ParseMessageStreamMessage(
        message_group.value(), message_code,
        base::span<uint8_t>(additional_data.begin(), additional_data.end()));

    // Only add a completely parsed message to the return vector.
    if (message)
      parsed_messages.push_back(std::move(message));
  }

  if (!remaining_bytes.empty()) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": " << remaining_bytes.size()
                                 << " remaining bytes not parsed.";
  }

  // TODO(jackshira): Handle partial message data by returning the amount read.
  std::move(callback).Run(std::move(parsed_messages));
}

// https://developers.google.com/nearby/fast-pair/spec#MessageStream
mojom::MessageStreamMessagePtr FastPairDataParser::ParseMessageStreamMessage(
    mojom::MessageGroup message_group,
    uint8_t message_code,
    const base::span<uint8_t>& additional_data) {
  switch (message_group) {
    case mojom::MessageGroup::kBluetoothEvent:
      if (!additional_data.empty())
        return nullptr;
      return ParseBluetoothEvent(message_code);
    case mojom::MessageGroup::kCompanionAppEvent:
      if (!additional_data.empty())
        return nullptr;
      return ParseCompanionAppEvent(message_code);
    case mojom::MessageGroup::kDeviceInformationEvent:
      return ParseDeviceInformationEvent(message_code,
                                         std::move(additional_data));
    case mojom::MessageGroup::kDeviceActionEvent:
      return ParseDeviceActionEvent(message_code, std::move(additional_data));
    case mojom::MessageGroup::kAcknowledgementEvent:
      return ParseAcknowledgementEvent(message_code,
                                       std::move(additional_data));
  }
}

// https://developers.google.com/nearby/fast-pair/spec#SilenceMode
mojom::MessageStreamMessagePtr FastPairDataParser::ParseBluetoothEvent(
    uint8_t message_code) {
  if (message_code == kEnableSilenceModeCode) {
    return mojom::MessageStreamMessage::NewEnableSilenceMode(true);
  }

  if (message_code == kDisableSilenceModeCode) {
    return mojom::MessageStreamMessage::NewEnableSilenceMode(false);
  }

  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": Unknown message code. Received 0x" << std::hex
      << message_code << ".";
  return nullptr;
}

// https://developers.google.com/nearby/fast-pair/spec#companion_app_events
mojom::MessageStreamMessagePtr FastPairDataParser::ParseCompanionAppEvent(
    uint8_t message_code) {
  if (message_code == kLogBufferFullCode) {
    return mojom::MessageStreamMessage::NewCompanionAppLogBufferFull(true);
  }

  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": Unknown message code. Received 0x" << std::hex
      << message_code << ".";
  return nullptr;
}

// https://developers.google.com/nearby/fast-pair/spec#MessageStreamDeviceInformation
mojom::MessageStreamMessagePtr FastPairDataParser::ParseDeviceInformationEvent(
    uint8_t message_code,
    const base::span<uint8_t>& additional_data) {
  if (message_code == kModelIdCode) {
    // Missing additional data containing model id value, since valid model id
    // will be length 3.
    if (additional_data.size() != 3) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of additional data bytes to parse "
             "model id Needed 3, received "
          << additional_data.size();
      return nullptr;
    }

    return mojom::MessageStreamMessage::NewModelId(
        base::HexEncode(additional_data));
  }

  if (message_code == kBleAddressUpdatedCode) {
    // Missing additional data containing ble address updated value, which will
    // be 6 bytes to be valid
    if (additional_data.size() != 6) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of additional data bytes to parse BLE "
             "address. Needed 6, received "
          << additional_data.size();
      return nullptr;
    }

    std::array<uint8_t, 6> address_bytes;
    base::ranges::copy(additional_data, address_bytes.begin());

    return mojom::MessageStreamMessage::NewBleAddressUpdate(
        device::CanonicalizeBluetoothAddress(address_bytes));
  }

  if (message_code == kBatteryUpdatedCode) {
    // Missing additional data containing battery updated value, since valid
    // battery update size will be length 3
    if (additional_data.size() != 3) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of additional data bytes to parse "
             "battery update. Needed 3, received "
          << additional_data.size();
      return nullptr;
    }

    // The additional data contains information about the battery components
    // about whether or not it is charging, and the battery percent. If
    // the percent is invalid (outside of range 0-100), then it is set to -1.
    mojom::BatteryUpdatePtr battery_update = mojom::BatteryUpdate::New();
    battery_update->left_bud_info = CreateBatteryInfo(additional_data[0]);
    battery_update->right_bud_info = CreateBatteryInfo(additional_data[1]);
    battery_update->case_info = CreateBatteryInfo(additional_data[2]);

    return mojom::MessageStreamMessage::NewBatteryUpdate(
        std::move(battery_update));
  }

  if (message_code == kRemainingBatteryTimeCode) {
    // Additional data contains the remaining battery time and will be 1 or 2
    // bytes.
    if (additional_data.size() != 1 && additional_data.size() != 2) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of additional data bytes to parse "
             "remaining battery time. Needed 1 or 2, received "
          << additional_data.size();
      return nullptr;
    }

    // If we have a single byte of remaining battery time, we can just set it
    // as the remaining battery time.
    if (additional_data.size() == 1) {
      uint16_t remaining_battery_time{additional_data[0]};
      return mojom::MessageStreamMessage::NewRemainingBatteryTime(
          remaining_battery_time);
    }

    // If we have two bytes of remaining battery time, then we need to combine
    // the bytes together to create a uint16_t
    uint16_t remaining_battery_time{additional_data[1]};
    uint16_t byte_to_shift{additional_data[0]};
    byte_to_shift = byte_to_shift << 8;
    remaining_battery_time |= byte_to_shift;
    return mojom::MessageStreamMessage::NewRemainingBatteryTime(
        remaining_battery_time);
  }

  if (message_code == kActiveComponentsResponseCode) {
    // Additional data contains the active components response, which for a
    // single component, will be 0 or 1 depending on whether or not it is
    // available, and for a device with multiple components, each bit in the
    // additional data represents whether that component is active. It is the
    // responsibility for consumers of the MessageStream to determine what
    // the meaning is, since a value of 0x01 can mean either a single component
    // is active or only the right bud is active, depending on the type of
    // device, which the MessageStream does not contain information
    // differentiating the two. See
    // https://developers.google.com/nearby/fast-pair/spec#MessageStreamActiveComponents
    if (additional_data.size() != 1) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of additional data bytes to parse "
             "active components. Needed 1, received "
          << additional_data.size();
      return nullptr;
    }

    return mojom::MessageStreamMessage::NewActiveComponentsByte(
        additional_data[0]);
  }

  if (message_code == kPlatformTypeCode) {
    // The additional data contains information about the Platform Type. For
    // now, the only platform that is supported is Android, but it may be
    // expanded in the future. The additional data will be 2 bytes to be valid:
    // the first byte to describe the platform, and the second byte is
    // customized per platform. For now, it describes the SDK version for
    // android, but when other platforms are supported, this will need to be
    // expanded on. See
    // https://developers.google.com/nearby/fast-pair/spec#PlatformType
    if (additional_data.size() != 2) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Not enough additional data bytes to parse platform "
             "type. Needed 2, received "
          << additional_data.size();
      return nullptr;
    }

    if (additional_data[0] != kAndroidPlatform) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Unknown platform type for MessageStreamMessage. Received 0x"
          << std::hex << additional_data[0];
      return nullptr;
    }

    int sdk_version = additional_data[1];
    return mojom::MessageStreamMessage::NewSdkVersion(sdk_version);
  }

  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": Unknown message code. Received 0x" << std::hex
      << message_code << ".";
  return nullptr;
}

// https://developers.google.com/nearby/fast-pair/spec#DeviceAction
mojom::MessageStreamMessagePtr FastPairDataParser::ParseDeviceActionEvent(
    uint8_t message_code,
    const base::span<uint8_t>& additional_data) {
  // There is only one device action supported for Fast Pair: ringing a device.
  // This can be updated when there are more device actions supported.
  // Ringing a device information is contained in the first byte in additional
  // information, and it can dictate either a single component or devices with
  // multiple components. The message stream does not contain information about
  // if the device is a single component or multiple components, so it is the
  // responsibility of the mojom::MessageStreamMessage consumer to determine
  // what the ring value means. See
  // https://developers.google.com/nearby/fast-pair/spec#ringing_a_device
  if (additional_data.size() != 1 && additional_data.size() != 2) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Invalid number of additional data bytes to parse "
           "device action. Needed 1 or 2, received "
        << additional_data.size();
    return nullptr;
  }

  if (message_code != kRingCode) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Unknown message code to parse DeviceAction code. Received 0x"
        << std::hex << message_code << ".";
    return nullptr;
  }

  mojom::RingDevicePtr ring_device = mojom::RingDevice::New();
  ring_device->ring_device_byte = additional_data[0];

  // Optional timeout field for ringing. Set to -1 if doesn't exist.
  if (additional_data.size() == 2)
    ring_device->timeout_in_seconds = static_cast<int>(additional_data[1]);
  else
    ring_device->timeout_in_seconds = -1;

  return mojom::MessageStreamMessage::NewRingDeviceEvent(
      std::move(ring_device));
}

// https://developers.google.com/nearby/fast-pair/spec#MessageStreamAcknowledgements
mojom::MessageStreamMessagePtr FastPairDataParser::ParseAcknowledgementEvent(
    uint8_t message_code,
    const base::span<uint8_t>& additional_data) {
  if (message_code != kAckCode && message_code != kNakCode) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Unknown message code to parse Acknowledgement code. Received 0x"
        << std::hex << message_code << ".";
    return nullptr;
  }

  if (message_code == kAckCode) {
    // The length two of additional data contains the action message group and
    // code.
    if (additional_data.size() != 2) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of bytes in additional data to parse "
             "Acknowledgement. Needed 2, received "
          << additional_data.size();
      return nullptr;
    }

    // Get the message group pertaining to the action being acknowledged.
    std::optional<mojom::MessageGroup> message_group =
        MessageGroupFromByte(additional_data[0]);
    if (!message_group.has_value()) {
      CD_LOG(WARNING, Feature::FP)
          << __func__ << ": Unknown message group. Received 0x" << std::hex
          << additional_data[0] << ".";
      return nullptr;
    }

    mojom::AcknowledgementMessagePtr ack = mojom::AcknowledgementMessage::New();
    ack->action_message_code = additional_data[1];
    ack->acknowledgement = mojom::Acknowledgement::kAck;
    ack->action_message_group = message_group.value();

    return mojom::MessageStreamMessage::NewAcknowledgement(std::move(ack));
  }

  if (message_code == kNakCode) {
    // The length three of additional data contains the action message group and
    // code.
    if (additional_data.size() != 3) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Invalid number of bytes in additional data to parse "
             "Acknowledgement. Needed 3, received "
          << additional_data.size();
      return nullptr;
    }

    std::optional<mojom::Acknowledgement> nak_reason =
        NakReasonFromByte(additional_data[0]);
    if (!nak_reason) {
      CD_LOG(WARNING, Feature::FP)
          << __func__
          << ": Unknown nak reason to parse Acknowledgement. Received 0x"
          << std::hex << additional_data[0];
      return nullptr;
    }

    // Get the message group pertaining to the action being nacknowledged.
    std::optional<mojom::MessageGroup> message_group =
        MessageGroupFromByte(additional_data[1]);
    if (!message_group.has_value()) {
      CD_LOG(WARNING, Feature::FP)
          << __func__ << ": Unknown message group. Received 0x" << std::hex
          << additional_data[1] << ".";
      return nullptr;
    }

    mojom::AcknowledgementMessagePtr ack = mojom::AcknowledgementMessage::New();
    ack->action_message_code = additional_data[2];
    ack->action_message_group = message_group.value();
    ack->acknowledgement = nak_reason.value();

    return mojom::MessageStreamMessage::NewAcknowledgement(std::move(ack));
  }

  return nullptr;
}

}  // namespace quick_pair
}  // namespace ash
