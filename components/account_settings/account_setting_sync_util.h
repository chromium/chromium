// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_UTIL_H_
#define COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_UTIL_H_

#include <string_view>
#include <variant>

#include "base/values.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"

namespace account_settings {

base::Value SettingSpecificsToValue(
    const sync_pb::AccountSettingSpecifics& specifics);

sync_pb::AccountSettingSpecifics CreateSettingSpecifics(
    std::string_view name,
    std::variant<bool, std::string_view, int64_t> value);

}  // namespace account_settings

#endif  // COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_UTIL_H_
