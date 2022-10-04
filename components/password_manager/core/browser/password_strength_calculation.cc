// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_strength_calculation.h"

namespace password_manager {

PasswordStrengthCalculation::PasswordStrengthCalculation() = default;
PasswordStrengthCalculation::~PasswordStrengthCalculation() = default;

void PasswordStrengthCalculation::SetServiceForTesting(
    mojo::PendingRemote<mojom::PasswordStrengthCalculator> calculator) {
  calculator_.Bind(std::move(calculator));
}

void PasswordStrengthCalculation::CheckPasswordWeakInSandbox(
    const std::string& password,
    CompletionCallback completion) {
  GetCalculator()->IsPasswordWeak(password, std::move(completion));
}

const mojo::Remote<mojom::PasswordStrengthCalculator>&
PasswordStrengthCalculation::GetCalculator() {
  if (!calculator_) {
    calculator_ = LaunchPasswordStrengthCalculator();
    calculator_.reset_on_disconnect();
  }
  return calculator_;
}

}  // namespace password_manager
