// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/run_on_os_login_types.h"

namespace apps {

namespace {

const char kLoginModeKey[] = "login_mode";
const char kIsManagedKey[] = "is_managed";

}  // namespace

APP_ENUM_TO_STRING(RunOnOsLoginMode, kUnknown, kNotRun, kWindowed)

RunOnOsLogin::RunOnOsLogin() = default;

RunOnOsLogin::RunOnOsLogin(RunOnOsLoginMode login_mode, bool is_managed)
    : login_mode(login_mode), is_managed(is_managed) {}

RunOnOsLogin::~RunOnOsLogin() = default;

bool RunOnOsLogin::operator==(const RunOnOsLogin& other) const {
  return login_mode == other.login_mode && is_managed == other.is_managed;
}

bool RunOnOsLogin::operator!=(const RunOnOsLogin& other) const {
  return !(*this == other);
}

base::Value::Dict ConvertRunOnOsLoginToDict(
    const RunOnOsLogin& run_on_os_login) {
  base::Value::Dict dict;
  dict.Set(kLoginModeKey, static_cast<int>(run_on_os_login.login_mode));
  dict.Set(kIsManagedKey, run_on_os_login.is_managed);
  return dict;
}

std::optional<RunOnOsLogin> ConvertDictToRunOnOsLogin(
    const base::Value::Dict* dict) {
  if (!dict) {
    return std::nullopt;
  }

  std::optional<int> login_mode = dict->FindInt(kLoginModeKey);
  if (!login_mode.has_value() ||
      login_mode.value() < static_cast<int>(RunOnOsLoginMode::kUnknown) ||
      login_mode.value() > static_cast<int>(RunOnOsLoginMode::kMaxValue)) {
    return std::nullopt;
  }

  std::optional<bool> is_managed = dict->FindBool(kIsManagedKey);
  if (!is_managed.has_value()) {
    return std::nullopt;
  }

  return RunOnOsLogin(static_cast<RunOnOsLoginMode>(login_mode.value()),
                      is_managed.value());
}

}  // namespace apps