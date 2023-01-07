// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_PASSWORD_STRENGTH_PASSWORD_STRENGTH_CALCULATOR_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_PASSWORD_STRENGTH_PASSWORD_STRENGTH_CALCULATOR_IMPL_H_

#include <string>

#include "components/password_manager/services/password_strength/public/mojom/password_strength_calculator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace password_manager {

// Implementation of the PasswordStrengthCalculator mojom interface.
class PasswordStrengthCalculatorImpl
    : public mojom::PasswordStrengthCalculator {
 public:
  // Constructs a PasswordStrengthCalculatorImpl bound to |receiver|.
  explicit PasswordStrengthCalculatorImpl(
      mojo::PendingReceiver<mojom::PasswordStrengthCalculator> receiver);
  PasswordStrengthCalculatorImpl(const PasswordStrengthCalculatorImpl&) =
      delete;
  PasswordStrengthCalculatorImpl& operator=(
      const PasswordStrengthCalculatorImpl&) = delete;
  ~PasswordStrengthCalculatorImpl() override;

  // password_manager::mojom::PasswordStrengthCalculator:
  void IsPasswordWeak(const std::string& password,
                      IsPasswordWeakCallback callback) override;

 private:
  mojo::Receiver<mojom::PasswordStrengthCalculator> receiver_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_PASSWORD_STRENGTH_PASSWORD_STRENGTH_CALCULATOR_IMPL_H_
