// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRENGTH_CALCULATION_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRENGTH_CALCULATION_H_

#include "components/password_manager/services/password_strength/password_strength_calculator_service.h"

namespace password_manager {

// Calculates the strength of a password in a utility process.
class PasswordStrengthCalculation {
 public:
  using CompletionCallback =
      mojom::PasswordStrengthCalculator::IsPasswordWeakCallback;

  PasswordStrengthCalculation();
  PasswordStrengthCalculation(const PasswordStrengthCalculation&) = delete;
  PasswordStrengthCalculation& operator=(const PasswordStrengthCalculation&) =
      delete;
  ~PasswordStrengthCalculation();

  // Overrides the password strength calculator service for testing.
  void SetServiceForTesting(
      mojo::PendingRemote<mojom::PasswordStrengthCalculator> calculator);

  // Calculates the `password` strength using a mojo sandbox process and
  // asynchronously calls `completion` with the result.
  void CheckPasswordWeakInSandbox(const std::string& password,
                                  CompletionCallback completion);

 private:
  const mojo::Remote<mojom::PasswordStrengthCalculator>& GetCalculator();

  mojo::Remote<mojom::PasswordStrengthCalculator> calculator_;

  base::WeakPtrFactory<PasswordStrengthCalculation> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRENGTH_CALCULATION_H_
