// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_textfield.h"

#include <utility>

namespace payments {

ValidatingTextfield::ValidatingTextfield(
    std::unique_ptr<ValidationDelegate> delegate)
    : Textfield(), delegate_(std::move(delegate)) {}

ValidatingTextfield::~ValidatingTextfield() {}

void ValidatingTextfield::OnBlur() {
  Textfield::OnBlur();
  was_blurred_ = true;

  // Do not validate if the view is being removed.
  if (!being_removed_)
    Validate();

  if (!GetText().empty() && delegate_->ShouldFormat())
    SetText(delegate_->Format(GetText()));
}

void ValidatingTextfield::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.child == this && !details.is_add)
    being_removed_ = true;
}

void ValidatingTextfield::OnContentsChanged() {
  // This is called on every keystroke.
  if (!GetText().empty() && GetCursorPosition() == GetText().length() &&
      delegate_->ShouldFormat()) {
    SetText(delegate_->Format(GetText()));
  }

  Validate();
}

bool ValidatingTextfield::IsValid() {
  base::string16 unused;
  return delegate_->IsValidTextfield(this, &unused);
}

void ValidatingTextfield::Validate() {
  // TextfieldValueChanged may have side-effects, such as displaying errors.
  SetInvalid(!delegate_->TextfieldValueChanged(this, was_blurred_));
}

}  // namespace payments
