// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class ValidatingTextfield : public views::Textfield {
 public:
  explicit ValidatingTextfield(std::unique_ptr<ValidationDelegate> delegate);
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

  DISALLOW_COPY_AND_ASSIGN(ValidatingTextfield);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_
