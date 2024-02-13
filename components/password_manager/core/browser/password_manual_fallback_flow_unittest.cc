// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using autofill::TestAutofillClient;

class PasswordManualFallbackFlowTest : public ::testing::Test {
 public:
  PasswordManualFallbackFlowTest()
      : flow_(&driver(), &autofill_client(), &password_manager_client()) {}

  PasswordManualFallbackFlow& flow() { return flow_; }

  StubPasswordManagerDriver& driver() { return driver_; }

  TestAutofillClient& autofill_client() { return autofill_client_; }

  StubPasswordManagerClient& password_manager_client() {
    return password_manager_client_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  StubPasswordManagerDriver driver_;
  TestAutofillClient autofill_client_;
  StubPasswordManagerClient password_manager_client_;
  PasswordManualFallbackFlow flow_;
};

TEST_F(PasswordManualFallbackFlowTest, RunFlow_NoSuggestionsShown) {
  flow().RunFlow();
  EXPECT_FALSE(autofill_client().IsShowingAutofillPopup());
}

}  // namespace password_manager
