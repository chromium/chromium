// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/password_strength/password_strength_calculator_impl.h"

#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/ui/weak_check_utility.h"

namespace password_manager {

PasswordStrengthCalculatorImpl::PasswordStrengthCalculatorImpl(
    mojo::PendingReceiver<mojom::PasswordStrengthCalculator> receiver)
    : receiver_(this, std::move(receiver)) {}

PasswordStrengthCalculatorImpl::~PasswordStrengthCalculatorImpl() = default;

void PasswordStrengthCalculatorImpl::IsPasswordWeak(
    const std::string& password,
    IsPasswordWeakCallback callback) {
  std::move(callback).Run(IsWeak(base::UTF8ToUTF16(password)).value());
}

}  // namespace password_manager
