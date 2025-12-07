// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_UTIL_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_UTIL_H_

#include <string_view>
#include <variant>

#include "components/sync/protocol/plus_address_setting_specifics.pb.h"

namespace plus_addresses {

sync_pb::PlusAddressSettingSpecifics CreateSettingSpecifics(
    std::string_view name,
    std::variant<bool, const char*, int32_t> value);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_UTIL_H_
