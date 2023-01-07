// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_NATIVE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_NATIVE_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

// The native views for AutofillErrorDialogView.
class AutofillErrorDialogViewNativeViews : public AutofillErrorDialogView,
                                           public views::DialogDelegateView {
 public:
  explicit AutofillErrorDialogViewNativeViews(
      AutofillErrorDialogController* controller);
  ~AutofillErrorDialogViewNativeViews() override;
  AutofillErrorDialogViewNativeViews(
      const AutofillErrorDialogViewNativeViews&) = delete;
  AutofillErrorDialogViewNativeViews& operator=(
      const AutofillErrorDialogViewNativeViews&) = delete;

  // AutofillErrorDialogView:
  void Dismiss() override;

  // DialogDelegate:
  views::View* GetContentsView() override;
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;

 private:
  raw_ptr<AutofillErrorDialogController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_AUTOFILL_ERROR_DIALOG_VIEW_NATIVE_VIEWS_H_
