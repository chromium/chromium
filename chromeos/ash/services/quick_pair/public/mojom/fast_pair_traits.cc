// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_traits.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// static
bool StructTraits<DecryptedResponseDataView, DecryptedResponse>::Read(
    DecryptedResponseDataView data,
    DecryptedResponse* out) {
  std::vector<uint8_t> address_bytes;
  if (!data.ReadAddressBytes(&address_bytes) ||
      address_bytes.size() != out->address_bytes.size())
    return false;

  std::vector<uint8_t> salt_bytes;
  if (!data.ReadSalt(&salt_bytes) || salt_bytes.size() != out->salt.size())
    return false;

  if (!EnumTraits<MessageType, FastPairMessageType>::FromMojom(
          data.message_type(), &out->message_type))
    return false;

  if (!data.ReadSecondaryAddressBytes(&out->secondary_address_bytes)) {
    return false;
  }

  base::ranges::copy(address_bytes, out->address_bytes.begin());
  base::ranges::copy(salt_bytes, out->salt.begin());
  out->flags = data.flags();
  out->num_addresses = data.num_addresses();

  return true;
}

// static
bool StructTraits<DecryptedPasskeyDataView, DecryptedPasskey>::Read(
    DecryptedPasskeyDataView data,
    DecryptedPasskey* out) {
  std::vector<uint8_t> salt_bytes;
  if (!data.ReadSalt(&salt_bytes) || salt_bytes.size() != out->salt.size())
    return false;

  if (!EnumTraits<MessageType, FastPairMessageType>::FromMojom(
          data.message_type(), &out->message_type))
    return false;

  out->passkey = data.passkey();
  base::ranges::copy(salt_bytes, out->salt.begin());

  return true;
}

// static
MessageType EnumTraits<MessageType, FastPairMessageType>::ToMojom(
    FastPairMessageType input) {
  switch (input) {
    case FastPairMessageType::kKeyBasedPairingRequest:
      return MessageType::kKeyBasedPairingRequest;
    case FastPairMessageType::kKeyBasedPairingResponse:
      return MessageType::kKeyBasedPairingResponse;
    case FastPairMessageType::kSeekersPasskey:
      return MessageType::kSeekersPasskey;
    case FastPairMessageType::kProvidersPasskey:
      return MessageType::kProvidersPasskey;
  }
}

// static
bool EnumTraits<MessageType, FastPairMessageType>::FromMojom(
    MessageType input,
    FastPairMessageType* out) {
  switch (input) {
    case MessageType::kKeyBasedPairingRequest:
      *out = FastPairMessageType::kKeyBasedPairingRequest;
      return true;
    case MessageType::kKeyBasedPairingResponse:
      *out = FastPairMessageType::kKeyBasedPairingResponse;
      return true;
    case MessageType::kSeekersPasskey:
      *out = FastPairMessageType::kSeekersPasskey;
      return true;
    case MessageType::kProvidersPasskey:
      *out = FastPairMessageType::kProvidersPasskey;
      return true;
  }

  return false;
}

// static
bool StructTraits<BatteryInfoDataView, BatteryInfo>::Read(
    BatteryInfoDataView data,
    BatteryInfo* out) {
  if (data.percentage() < -1 || data.percentage() > 100)
    return false;

  out->is_charging = data.is_charging();
  out->percentage = data.percentage() == -1
                        ? std::nullopt
                        : std::make_optional(data.percentage());

  return true;
}

// static
bool StructTraits<BatteryNotificationDataView, BatteryNotification>::Read(
    BatteryNotificationDataView data,
    BatteryNotification* out) {
  if (!data.ReadLeftBudInfo(&out->left_bud_info) ||
      !data.ReadRightBudInfo(&out->right_bud_info) ||
      !data.ReadCaseInfo(&out->case_info)) {
    return false;
  }

  out->show_ui = data.show_ui();

  return true;
}

// static
bool StructTraits<NotDiscoverableAdvertisementDataView,
                  NotDiscoverableAdvertisement>::
    Read(NotDiscoverableAdvertisementDataView data,
         NotDiscoverableAdvertisement* out) {
  if (!data.ReadAccountKeyFilter(&out->account_key_filter))
    return false;

  if (!data.ReadBatteryNotification(&out->battery_notification))
    return false;

  if (!data.ReadSalt(&out->salt))
    return false;

  if (out->salt.size() != 1 && out->salt.size() != 2 && out->salt.size() != 6)
    return false;

  out->show_ui = data.show_ui();
  return true;
}

}  // namespace mojo
