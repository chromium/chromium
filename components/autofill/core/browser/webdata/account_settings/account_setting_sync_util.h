// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_UTIL_H_

#include <string_view>
#include <variant>

#include "components/sync/protocol/account_setting_specifics.pb.h"

namespace autofill {

sync_pb::AccountSettingSpecifics CreateSettingSpecifics(
    std::string_view name,
    std::variant<bool, std::string_view, int64_t> value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_UTIL_H_
