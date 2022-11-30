// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/password_strength/password_strength_calculator_impl.h"

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class PasswordStrengthCalculatorTest : public testing::Test {
 protected:
  PasswordStrengthCalculatorTest()
      : calculator_(
            mojo::PendingReceiver<mojom::PasswordStrengthCalculator>()) {}

  void IsPasswordWeak(
      const std::string& password,
      PasswordStrengthCalculatorImpl::IsPasswordWeakCallback callback) {
    calculator_.IsPasswordWeak(password, std::move(callback));
  }

 private:
  PasswordStrengthCalculatorImpl calculator_;
};

TEST_F(PasswordStrengthCalculatorTest,
       CalculatingEmptyPasswordStrengthReturnsWeak) {
  IsPasswordWeak("", base::BindLambdaForTesting(
                         [](bool is_weak) { EXPECT_TRUE(is_weak); }));
}

TEST_F(PasswordStrengthCalculatorTest,
       CalculatingNonEmptyPasswordStrengthReturnsWeak) {
  IsPasswordWeak("aaa", base::BindLambdaForTesting(
                            [](bool is_weak) { EXPECT_TRUE(is_weak); }));
}

TEST_F(PasswordStrengthCalculatorTest,
       CalculatingNonEmptyPasswordStrengthReturnsStrong) {
  IsPasswordWeak(
      "W9iFUTzEJEQrQ5t",
      base::BindLambdaForTesting([](bool is_weak) { EXPECT_FALSE(is_weak); }));
}

}  // namespace password_manager
