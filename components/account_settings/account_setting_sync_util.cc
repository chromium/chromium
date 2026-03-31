// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_settings/account_setting_sync_util.h"

#include <string>
#include <variant>

#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace account_settings {

base::Value SettingSpecificsToValue(
    const sync_pb::AccountSettingSpecifics& specifics) {
  switch (specifics.Value_case()) {
    case sync_pb::AccountSettingSpecifics::kBoolValue:
      return base::Value(specifics.bool_value());
    case sync_pb::AccountSettingSpecifics::kStringValue:
      return base::Value(specifics.string_value());
    case sync_pb::AccountSettingSpecifics::kIntValue:
      return base::Value(static_cast<int>(specifics.int_value()));
    case sync_pb::AccountSettingSpecifics::VALUE_NOT_SET:
      return base::Value();
  }
}

sync_pb::AccountSettingSpecifics CreateSettingSpecifics(
    std::string_view name,
    std::variant<bool, std::string_view, int64_t> value) {
  sync_pb::AccountSettingSpecifics specifics;
  specifics.set_name(std::string(name));
  std::visit(
      absl::Overload{
          [&](bool value) { specifics.set_bool_value(value); },
          [&](std::string_view value) { specifics.set_string_value(value); },
          [&](int64_t value) { specifics.set_int_value(value); }},
      value);
  return specifics;
}

}  // namespace account_settings
