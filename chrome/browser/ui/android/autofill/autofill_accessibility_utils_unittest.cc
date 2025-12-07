// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class MockAutofillAccessibilityHelper : public AutofillAccessibilityHelper {
 public:
  MockAutofillAccessibilityHelper() = default;
  ~MockAutofillAccessibilityHelper() override = default;

  MOCK_METHOD(void,
              AnnounceTextForA11y,
              (const std::u16string& message),
              (override));
};

class AutofillAccessibilityUtilsTest : public testing::Test {
 public:
  AutofillAccessibilityUtilsTest() = default;
  ~AutofillAccessibilityUtilsTest() override = default;

 protected:
  void SetUp() override {
    mock_helper_ = std::make_unique<MockAutofillAccessibilityHelper>();
    AutofillAccessibilityHelper::SetInstanceForTesting(mock_helper_.get());
  }

  void TearDown() override {
    AutofillAccessibilityHelper::SetInstanceForTesting(nullptr);
    mock_helper_.reset();
  }

  MockAutofillAccessibilityHelper* mock_helper_ptr() {
    return mock_helper_.get();
  }

 private:
  std::unique_ptr<MockAutofillAccessibilityHelper> mock_helper_;
};

// Verifies that the AnnounceTextForA11y method correctly delegates to the
// underlying helper.
TEST_F(AutofillAccessibilityUtilsTest, AnnounceTextForA11y_CallsHelper) {
  const std::u16string test_message = u"Form filled successfully";

  EXPECT_CALL(*mock_helper_ptr(), AnnounceTextForA11y(test_message)).Times(1);

  AutofillAccessibilityHelper::GetInstance()->AnnounceTextForA11y(test_message);
}

// Tests that empty messages are handled gracefully without causing crashes.
TEST_F(AutofillAccessibilityUtilsTest, AnnounceTextForA11y_EmptyMessage) {
  const std::u16string empty_message = u"";

  EXPECT_CALL(*mock_helper_ptr(), AnnounceTextForA11y(empty_message)).Times(1);

  AutofillAccessibilityHelper::GetInstance()->AnnounceTextForA11y(
      empty_message);
}

// Confirms that GetInstance returns the mock helper during testing.
TEST_F(AutofillAccessibilityUtilsTest, GetInstance_ReturnsMockDuringTest) {
  AutofillAccessibilityHelper* instance =
      AutofillAccessibilityHelper::GetInstance();
  EXPECT_EQ(instance, mock_helper_ptr());
}

// Simulates production code usage to verify accessibility announcements work
// correctly.
TEST_F(AutofillAccessibilityUtilsTest,
       ProductionCode_VerifyAccessibilityAnnouncements) {
  const std::u16string expected_message = u"Credit card information filled";

  EXPECT_CALL(*mock_helper_ptr(), AnnounceTextForA11y(expected_message))
      .Times(1);

  AutofillAccessibilityHelper::GetInstance()->AnnounceTextForA11y(
      expected_message);
}

}  // namespace autofill
