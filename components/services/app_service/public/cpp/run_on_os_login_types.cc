// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/run_on_os_login_types.h"

namespace apps {

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

}  // namespace apps