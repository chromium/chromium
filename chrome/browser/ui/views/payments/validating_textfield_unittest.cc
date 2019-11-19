// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_textfield.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class ValidatingTextfieldTest : public ChromeViewsTestBase {
 public:
  ValidatingTextfieldTest() {}
  ~ValidatingTextfieldTest() override {}

 protected:
  class TestValidationDelegate : public ValidationDelegate {
   public:
    TestValidationDelegate() {}
    ~TestValidationDelegate() override {}

    // ValidationDelegate:
    bool TextfieldValueChanged(views::Textfield* textfield,
                               bool was_blurred) override {
      base::string16 unused;
      return !was_blurred || IsValidTextfield(textfield, &unused);
    }
    bool ComboboxValueChanged(views::Combobox* combobox) override {
      base::string16 unused;
      return IsValidCombobox(combobox, &unused);
    }
    bool IsValidTextfield(views::Textfield* textfield,
                          base::string16* error_message) override {
      // We really don't like textfields with more than 5 characters in them.
      return textfield->GetText().size() <= 5u;
    }
    bool IsValidCombobox(views::Combobox* combobox,
                         base::string16* error_message) override {
      return true;
    }
    void ComboboxModelChanged(views::Combobox* combobox) override {}

   private:
    DISALLOW_COPY_AND_ASSIGN(TestValidationDelegate);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(ValidatingTextfieldTest);
};

TEST_F(ValidatingTextfieldTest, Validation) {
  std::unique_ptr<TestValidationDelegate> delegate(
      new TestValidationDelegate());
  std::unique_ptr<ValidatingTextfield> textfield(
      new ValidatingTextfield(std::move(delegate)));

  // Set an invalid string (>5 characters).
  textfield->SetText(base::ASCIIToUTF16("evilstring"));
  // This should be called on new contents by the textfield controller.
  textfield->OnContentsChanged();

  // Not marked as invalid.
  EXPECT_FALSE(textfield->GetInvalid());

  // On blur though, there is a first validation.
  textfield->OnBlur();
  EXPECT_TRUE(textfield->GetInvalid());

  // On further text adjustements, the validation runs now. Set a valid string
  // (<=5 characters).
  textfield->SetText(base::ASCIIToUTF16("good"));
  textfield->OnContentsChanged();

  // No longer invalid.
  EXPECT_FALSE(textfield->GetInvalid());
}

}  // namespace payments
