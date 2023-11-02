// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class Throbber;
}  // namespace views

namespace autofill {

// The native views for AutofillProgressDialogView.
class AutofillProgressDialogViews : public AutofillProgressDialogView,
                                    public views::DialogDelegateView {
 public:
  explicit AutofillProgressDialogViews(
      AutofillProgressDialogController* controller);
  AutofillProgressDialogViews(const AutofillProgressDialogViews&) = delete;
  AutofillProgressDialogViews& operator=(const AutofillProgressDialogViews&) =
      delete;
  ~AutofillProgressDialogViews() override;

  // AutofillProgressDialogView:
  void Dismiss(bool show_confirmation_before_closing,
               bool is_canceled_by_user) override;

  // DialogDelegate:
  void AddedToWidget() override;
  void OnThemeChanged() override;
  std::u16string GetWindowTitle() const override;

 private:
  // Close the widget of this view, and notify controller. |is_canceled_by_user|
  // is a boolean that is true if the user cancels the progress dialog, false if
  // the progress dialog closes automatically after a confirmation message.
  void CloseWidget(bool is_canceled_by_user);

  raw_ptr<AutofillProgressDialogController> controller_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Throbber> progress_throbber_ = nullptr;

  base::WeakPtrFactory<AutofillProgressDialogViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_VIEWS_H_
