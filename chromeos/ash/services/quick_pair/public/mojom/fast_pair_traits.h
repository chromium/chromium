// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_MOJOM_FAST_PAIR_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_MOJOM_FAST_PAIR_TRAITS_H_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "chromeos/ash/services/quick_pair/public/cpp/battery_notification.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace {
using ash::quick_pair::BatteryInfo;
using ash::quick_pair::BatteryNotification;
using ash::quick_pair::DecryptedPasskey;
using ash::quick_pair::DecryptedResponse;
using ash::quick_pair::FastPairMessageType;
using ash::quick_pair::NotDiscoverableAdvertisement;
using ash::quick_pair::mojom::BatteryInfoDataView;
using ash::quick_pair::mojom::BatteryNotificationDataView;
using ash::quick_pair::mojom::DecryptedPasskeyDataView;
using ash::quick_pair::mojom::DecryptedResponseDataView;
using ash::quick_pair::mojom::MessageType;
using ash::quick_pair::mojom::NotDiscoverableAdvertisementDataView;
}  // namespace

template <>
class EnumTraits<MessageType, FastPairMessageType> {
 public:
  static MessageType ToMojom(FastPairMessageType input);
  static bool FromMojom(MessageType input, FastPairMessageType* out);
};

template <>
class StructTraits<DecryptedResponseDataView, DecryptedResponse> {
 public:
  static MessageType message_type(const DecryptedResponse& r) {
    return EnumTraits<MessageType, FastPairMessageType>::ToMojom(
        r.message_type);
  }

  static std::vector<uint8_t> address_bytes(const DecryptedResponse& r) {
    return std::vector<uint8_t>(r.address_bytes.begin(), r.address_bytes.end());
  }

  static std::vector<uint8_t> salt(const DecryptedResponse& r) {
    return std::vector<uint8_t>(r.salt.begin(), r.salt.end());
  }

  static std::optional<uint8_t> flags(const DecryptedResponse& r) {
    return r.flags;
  }

  static std::optional<uint8_t> num_addresses(const DecryptedResponse& r) {
    return r.num_addresses;
  }

  static std::optional<std::vector<uint8_t>> secondary_address_bytes(
      const DecryptedResponse& r) {
    if (!r.secondary_address_bytes.has_value()) {
      return std::nullopt;
    }
    return std::vector<uint8_t>(r.secondary_address_bytes.value().begin(),
                                r.secondary_address_bytes.value().end());
  }

  static bool Read(DecryptedResponseDataView data, DecryptedResponse* out);
};

template <>
class StructTraits<DecryptedPasskeyDataView, DecryptedPasskey> {
 public:
  static MessageType message_type(const DecryptedPasskey& r) {
    return EnumTraits<MessageType, FastPairMessageType>::ToMojom(
        r.message_type);
  }

  static uint32_t passkey(const DecryptedPasskey& r) { return r.passkey; }

  static std::vector<uint8_t> salt(const DecryptedPasskey& r) {
    return std::vector<uint8_t>(r.salt.begin(), r.salt.end());
  }

  static bool Read(DecryptedPasskeyDataView data, DecryptedPasskey* out);
};

template <>
class StructTraits<BatteryInfoDataView, BatteryInfo> {
 public:
  static bool is_charging(const BatteryInfo& r) { return r.is_charging; }

  static int8_t percentage(const BatteryInfo& r) {
    return r.percentage.value_or(-1);
  }

  static bool Read(BatteryInfoDataView data, BatteryInfo* out);
};

template <>
class StructTraits<BatteryNotificationDataView, BatteryNotification> {
 public:
  static bool show_ui(const BatteryNotification& r) { return r.show_ui; }

  static BatteryInfo left_bud_info(const BatteryNotification& r) {
    return r.left_bud_info;
  }

  static BatteryInfo right_bud_info(const BatteryNotification& r) {
    return r.right_bud_info;
  }

  static BatteryInfo case_info(const BatteryNotification& r) {
    return r.case_info;
  }

  static bool Read(BatteryNotificationDataView data, BatteryNotification* out);
};

template <>
class StructTraits<NotDiscoverableAdvertisementDataView,
                   NotDiscoverableAdvertisement> {
 public:
  static std::vector<uint8_t> account_key_filter(
      const NotDiscoverableAdvertisement& r) {
    return r.account_key_filter;
  }

  static bool show_ui(const NotDiscoverableAdvertisement& r) {
    return r.show_ui;
  }

  static std::vector<uint8_t> salt(const NotDiscoverableAdvertisement& r) {
    return r.salt;
  }

  static std::optional<BatteryNotification> battery_notification(
      const NotDiscoverableAdvertisement& r) {
    return r.battery_notification;
  }

  static bool Read(NotDiscoverableAdvertisementDataView data,
                   NotDiscoverableAdvertisement* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_MOJOM_FAST_PAIR_TRAITS_H_
