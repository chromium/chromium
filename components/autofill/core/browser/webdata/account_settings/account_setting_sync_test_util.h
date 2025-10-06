// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_TEST_UTIL_H_

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

// Matchers for `AccountSettingSpecifics` args with given name and values.
MATCHER_P2(HasBoolSetting, name, value, "") {
  return arg.name() == name && arg.has_bool_value() &&
         arg.bool_value() == value;
}
MATCHER_P2(HasStringSetting, name, value, "") {
  return arg.name() == name && arg.has_string_value() &&
         arg.string_value() == value;
}
MATCHER_P2(HasIntSetting, name, value, "") {
  return arg.name() == name && arg.has_int_value() && arg.int_value() == value;
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_TEST_UTIL_H_
