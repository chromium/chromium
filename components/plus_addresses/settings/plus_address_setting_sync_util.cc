// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_sync_util.h"

#include <string>

#include "base/functional/overloaded.h"

namespace plus_addresses {

sync_pb::PlusAddressSettingSpecifics CreateSettingSpecifics(
    std::string_view name,
    absl::variant<bool, const char*, int32_t> value) {
  sync_pb::PlusAddressSettingSpecifics specifics;
  specifics.set_name(std::string(name));
  absl::visit(base::Overloaded{
                  [&](bool value) { specifics.set_bool_value(value); },
                  [&](const char* value) { specifics.set_string_value(value); },
                  [&](int32_t value) { specifics.set_int_value(value); }},
              value);
  return specifics;
}

}  // namespace plus_addresses
