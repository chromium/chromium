// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Per-tab controller for the AutofillDialog.
class AutofillDialogControllerImpl : public AutofillDialogController {
 public:
  explicit AutofillDialogControllerImpl(content::WebContents* web_contents);
  ~AutofillDialogControllerImpl() override;

  AutofillDialogControllerImpl(const AutofillDialogControllerImpl&) = delete;
  AutofillDialogControllerImpl& operator=(const AutofillDialogControllerImpl&) =
      delete;

  // AutofillDialogController:
  void Show(const std::u16string& title,
            const std::u16string& description,
            const std::u16string& button_text,
            base::OnceClosure on_positive_button_clicked_callback) override;
  void ShowLoadingDialog(const std::u16string& title,
                         base::TimeDelta min_time) override;
  void Dismiss() override;

  void OnPositiveButtonClicked() override;
  void OnDismissed() override;
  std::u16string GetTitleText() const override;
  std::u16string GetDescriptionText() const override;
  std::u16string GetButtonText() const override;
  content::WebContents& GetWebContents() const override;

  // Method for tests to inject a mock or test view.
  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<AutofillDialogView>()>;
  void SetViewFactoryForTest(FactoryCallback view_factory_for_test) {
    view_factory_for_test_ = std::move(view_factory_for_test);
  }
  bool HasDialogViewForTest() const { return !!autofill_dialog_view_; }
  void DismissForTest() { Dismiss(); }

 private:
  raw_ref<content::WebContents> web_contents_;

  std::unique_ptr<AutofillDialogView> autofill_dialog_view_;

  std::u16string title_;
  std::u16string description_;
  std::u16string button_text_;

  base::OneShotTimer dismiss_timer_;
  base::ElapsedTimer dialog_show_time_;
  base::TimeDelta min_show_time_;

  // Callback to run after the dialog positive button is clicked.
  base::OnceClosure on_positive_button_clicked_callback_;

  FactoryCallback view_factory_for_test_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
