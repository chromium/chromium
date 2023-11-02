// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_textfield.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class ValidatingTextfieldTest : public ChromeViewsTestBase {
 public:
  ValidatingTextfieldTest() {}

  ValidatingTextfieldTest(const ValidatingTextfieldTest&) = delete;
  ValidatingTextfieldTest& operator=(const ValidatingTextfieldTest&) = delete;

  ~ValidatingTextfieldTest() override {}

 protected:
  class TestValidationDelegate : public ValidationDelegate {
   public:
    TestValidationDelegate() {}

    TestValidationDelegate(const TestValidationDelegate&) = delete;
    TestValidationDelegate& operator=(const TestValidationDelegate&) = delete;

    ~TestValidationDelegate() override {}

    // ValidationDelegate:
    bool TextfieldValueChanged(views::Textfield* textfield,
                               bool was_blurred) override {
      std::u16string unused;
      return !was_blurred || IsValidTextfield(textfield, &unused);
    }
    bool ComboboxValueChanged(ValidatingCombobox* combobox) override {
      std::u16string unused;
      return IsValidCombobox(combobox, &unused);
    }
    bool IsValidTextfield(views::Textfield* textfield,
                          std::u16string* error_message) override {
      // We really don't like textfields with more than 5 characters in them.
      return textfield->GetText().size() <= 5u;
    }
    bool IsValidCombobox(ValidatingCombobox* combobox,
                         std::u16string* error_message) override {
      return true;
    }
    void ComboboxModelChanged(ValidatingCombobox* combobox) override {}
  };
};

TEST_F(ValidatingTextfieldTest, Validation) {
  std::unique_ptr<TestValidationDelegate> delegate(
      new TestValidationDelegate());
  std::unique_ptr<ValidatingTextfield> textfield(
      new ValidatingTextfield(std::move(delegate)));

  // Set an invalid string (>5 characters).
  textfield->SetText(u"evilstring");
  // This should be called on new contents by the textfield controller.
  textfield->OnContentsChanged();

  // Not marked as invalid.
  EXPECT_FALSE(textfield->GetInvalid());

  // On blur though, there is a first validation.
  textfield->OnBlur();
  EXPECT_TRUE(textfield->GetInvalid());

  // On further text adjustements, the validation runs now. Set a valid string
  // (<=5 characters).
  textfield->SetText(u"good");
  textfield->OnContentsChanged();

  // No longer invalid.
  EXPECT_FALSE(textfield->GetInvalid());
}

}  // namespace payments
