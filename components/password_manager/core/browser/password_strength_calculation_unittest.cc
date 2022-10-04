// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_strength_calculation.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/ui/weak_check_utility.h"
#include "components/password_manager/services/password_strength/password_strength_calculator_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

// A wrapper class that mimics the password strength sandboxed behaviour.
class FakePasswordStrengthCalculatorService
    : public mojom::PasswordStrengthCalculator {
 public:
  explicit FakePasswordStrengthCalculatorService(
      scoped_refptr<base::TaskRunner> task_runner)
      : task_runner_(task_runner) {}

  void IsPasswordWeak(const std::string& password,
                      base::OnceCallback<void(bool)> callback) override {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::u16string password,
                          base::OnceCallback<void(bool)> callback) {
                         std::move(callback).Run(IsWeak(password).value());
                       },
                       base::UTF8ToUTF16(password), std::move(callback)));
  }

 private:
  scoped_refptr<base::TaskRunner> task_runner_;
};

class PasswordStrengthCalculationTest : public testing::Test {
 public:
  PasswordStrengthCalculationTest()
      : service_{task_environment_.GetMainThreadTaskRunner()},
        receiver_{&service_} {
    mojo::PendingRemote<mojom::PasswordStrengthCalculator> pending_remote{
        receiver_.BindNewPipeAndPassRemote()};
    calculation_.SetServiceForTesting(std::move(pending_remote));
  }

 protected:
  void CalculatePasswordStrengthAndWaitForCompletion(
      const std::string& password) {
    calculation_.CheckPasswordWeakInSandbox(
        password,
        base::BindOnce(
            &PasswordStrengthCalculationTest::OnPasswordStrengthCalculated,
            base::Unretained(this)));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(result_callback_called_);
  }

  bool IsPasswordWeak() const { return password_strength_result_; }

 private:
  void OnPasswordStrengthCalculated(bool is_weak) {
    result_callback_called_ = true;
    password_strength_result_ = is_weak;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  bool result_callback_called_ = false;
  bool password_strength_result_ = false;
  PasswordStrengthCalculation calculation_;
  FakePasswordStrengthCalculatorService service_;
  mojo::Receiver<mojom::PasswordStrengthCalculator> receiver_;
};

TEST_F(PasswordStrengthCalculationTest,
       CalculatingEmptyPasswordStrengthReturnsWeak) {
  CalculatePasswordStrengthAndWaitForCompletion("");
  EXPECT_TRUE(IsPasswordWeak());
}

TEST_F(PasswordStrengthCalculationTest,
       CalculatingNonEmptyPasswordStrengthReturnsWeak) {
  CalculatePasswordStrengthAndWaitForCompletion("aaa");
  EXPECT_TRUE(IsPasswordWeak());
}

TEST_F(PasswordStrengthCalculationTest,
       CalculatingNonEmptyPasswordStrengthReturnsStrong) {
  CalculatePasswordStrengthAndWaitForCompletion("aV4kPQ5qb7B7bpi");
  EXPECT_FALSE(IsPasswordWeak());
}

}  // namespace password_manager
