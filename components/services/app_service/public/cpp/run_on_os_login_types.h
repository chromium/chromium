// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_RUN_ON_OS_LOGIN_TYPES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_RUN_ON_OS_LOGIN_TYPES_H_

#include <utility>
#include <vector>

#include "base/component_export.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

ENUM_FOR_COMPONENT(LOGIN_MODE,
                   RunOnOsLoginMode,
                   // kUnknown to be used for app_update.cc.
                   kUnknown,
                   // App won't run on OS Login.
                   kNotRun,
                   // App runs in windowed mode on OS Login.
                   kWindowed)

struct COMPONENT_EXPORT(LOGIN_MODE) RunOnOsLogin {
  RunOnOsLogin();
  RunOnOsLogin(RunOnOsLoginMode login_mode, bool is_managed);

  RunOnOsLogin(const RunOnOsLogin&) = delete;
  RunOnOsLogin& operator=(const RunOnOsLogin&) = delete;
  RunOnOsLogin(RunOnOsLogin&&) = default;
  RunOnOsLogin& operator=(RunOnOsLogin&&) = default;

  bool operator==(const RunOnOsLogin& other) const;
  bool operator!=(const RunOnOsLogin& other) const;

  ~RunOnOsLogin();

  // RunOnOsLoginMode struct to be used
  // to verify if the mode is set by policy
  // or not.
  RunOnOsLoginMode login_mode;
  // If the run on os login mode is policy
  // controlled or not.
  bool is_managed;
};

using RunOnOsLoginPtr = std::unique_ptr<RunOnOsLogin>;

COMPONENT_EXPORT(LOGIN_MODE)
apps::mojom::RunOnOsLoginPtr ConvertRunOnOsLoginToMojomRunOnOsLogin(
    const RunOnOsLogin& run_on_os_login);

COMPONENT_EXPORT(LOGIN_MODE)
apps::mojom::RunOnOsLoginMode ConvertRunOnOsLoginModeToMojomRunOnOsLoginMode(
    RunOnOsLoginMode login_mode);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_RUN_ON_OS_LOGIN_TYPES_H_