// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_util.h"

#include <string>
#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

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

}  // namespace autofill
