// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_

#include <memory>

#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class ValidatingTextfield : public views::Textfield {
  METADATA_HEADER(ValidatingTextfield, views::Textfield)

 public:
  explicit ValidatingTextfield(std::unique_ptr<ValidationDelegate> delegate);
  ValidatingTextfield(const ValidatingTextfield&) = delete;
  ValidatingTextfield& operator=(const ValidatingTextfield&) = delete;
  ~ValidatingTextfield() override;

  // Textfield:
  // The first validation will happen on blur.
  void OnBlur() override;
  // Used to keep track of our own destruction.
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // Called when the textfield contents is changed. May do validation.
  void OnContentsChanged();

  // Identifies whether the current content if valid or not.
  bool IsValid();

 private:
  // Will call to the ValidationDelegate to validate the contents of the
  // textfield.
  void Validate();

  std::unique_ptr<ValidationDelegate> delegate_;
  bool was_blurred_ = false;
  bool being_removed_ = false;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_
