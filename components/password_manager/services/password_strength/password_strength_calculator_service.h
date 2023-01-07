// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_PASSWORD_STRENGTH_PASSWORD_STRENGTH_CALCULATOR_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_PASSWORD_STRENGTH_PASSWORD_STRENGTH_CALCULATOR_SERVICE_H_

#include "components/password_manager/services/password_strength/public/mojom/password_strength_calculator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace password_manager {

// Launches a new instance of the PasswordStrengthCalculator service in an
// isolated, sandboxed process, and returns a remote interface to control the
// service. The lifetime of the process is tied to that of the Remote. May be
// called from any thread.
mojo::Remote<password_manager::mojom::PasswordStrengthCalculator>
LaunchPasswordStrengthCalculator();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_PASSWORD_STRENGTH_PASSWORD_STRENGTH_CALCULATOR_SERVICE_H_
