// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_UTIL_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_UTIL_H_

#include <string_view>

#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace plus_addresses {

sync_pb::PlusAddressSettingSpecifics CreateSettingSpecifics(
    std::string_view name,
    absl::variant<bool, const char*, int32_t> value);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_UTIL_H_
