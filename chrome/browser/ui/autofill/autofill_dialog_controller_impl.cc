// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <string>

#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"

namespace autofill {

AutofillDialogControllerImpl::AutofillDialogControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

AutofillDialogControllerImpl::~AutofillDialogControllerImpl() {
  // If the tab is killed then dismiss the dialog if it's showing.
  Dismiss();
}

void AutofillDialogControllerImpl::Show(
    const std::u16string& title,
    const std::u16string& description,
    const std::u16string& button_text,
    base::OnceClosure on_positive_button_clicked_callback) {
  if (autofill_dialog_view_) {
    // A dialog is already showing. Ignore the new request.
    return;
  }

  title_ = title;
  description_ = description;
  button_text_ = button_text;
  on_positive_button_clicked_callback_ =
      std::move(on_positive_button_clicked_callback);

  if (view_factory_for_test_) {
    autofill_dialog_view_ = view_factory_for_test_.Run();
  } else {
    autofill_dialog_view_ = AutofillDialogView::Create(this);
  }
  autofill_dialog_view_->Show();
}

void AutofillDialogControllerImpl::OnPositiveButtonClicked() {
  std::move(on_positive_button_clicked_callback_).Run();
}

void AutofillDialogControllerImpl::OnDismissed() {
  autofill_dialog_view_.reset();
}

std::u16string AutofillDialogControllerImpl::GetTitleText() const {
  return title_;
}

std::u16string AutofillDialogControllerImpl::GetDescriptionText() const {
  return description_;
}

std::u16string AutofillDialogControllerImpl::GetButtonText() const {
  return button_text_;
}

content::WebContents& AutofillDialogControllerImpl::GetWebContents() const {
  return web_contents_.get();
}

void AutofillDialogControllerImpl::Dismiss() {
  if (!autofill_dialog_view_) {
    return;
  }

  autofill_dialog_view_->Dismiss();
  autofill_dialog_view_.reset();
}

}  // namespace autofill
