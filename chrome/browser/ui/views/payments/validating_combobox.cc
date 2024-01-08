// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_combobox.h"

#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"

namespace payments {

ValidatingCombobox::ValidatingCombobox(
    std::unique_ptr<ui::ComboboxModel> model,
    std::unique_ptr<ValidationDelegate> delegate)
    : Combobox(std::move(model)), delegate_(std::move(delegate)) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

ValidatingCombobox::~ValidatingCombobox() = default;

void ValidatingCombobox::OnBlur() {
  Combobox::OnBlur();

  // Validations will occur when the content changes. Do not validate if the
  // view is being removed.
  if (!being_removed_) {
    Validate();
  }
}

void ValidatingCombobox::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.child == this && !details.is_add)
    being_removed_ = true;
}

void ValidatingCombobox::OnContentsChanged() {
  Validate();
}

void ValidatingCombobox::OnComboboxModelChanged(ui::ComboboxModel* model) {
  views::Combobox::OnComboboxModelChanged(model);
  delegate_->ComboboxModelChanged(this);
}

bool ValidatingCombobox::IsValid() {
  std::u16string unused;
  return delegate_->IsValidCombobox(this, &unused);
}

void ValidatingCombobox::Validate() {
  // ComboboxValueChanged may have side-effects, such as displaying errors.
  SetInvalid(!delegate_->ComboboxValueChanged(this));
}

BEGIN_METADATA(ValidatingCombobox)
END_METADATA

}  // namespace payments
