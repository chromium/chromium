// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_CONTROLLER_IMPL_H_
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_type.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Per-tab controller for the AutofillSnackbar.
class AutofillSnackbarControllerImpl : public AutofillSnackbarController {
 public:
  explicit AutofillSnackbarControllerImpl(content::WebContents* web_contents);
  ~AutofillSnackbarControllerImpl() override;

  AutofillSnackbarControllerImpl(const AutofillSnackbarControllerImpl&) =
      delete;
  AutofillSnackbarControllerImpl& operator=(
      const AutofillSnackbarControllerImpl&) = delete;

  // The default duration for which the snackbar should be shown.
  static constexpr base::TimeDelta kDefaultSnackbarDuration = base::Seconds(10);

  // Shows the snackbar.
  virtual void Show(AutofillSnackbarType autofill_snackbar_type);

  // Similar to Show() but includes a duration and callback parameter. The
  // duration parameter controls how long the snackbar will be shown before it
  // is automatically dismissed. The callback parameter is an optional parameter
  // which is called when the snackbar is dismissed.
  virtual void ShowWithDurationAndCallback(
      AutofillSnackbarType autofill_snackbar_type,
      base::TimeDelta snackbar_duration,
      std::optional<base::OnceClosure> on_dismiss_callback);

  // AutofillSnackbarController:
  void OnActionClicked() override;
  void OnDismissed() override;
  std::u16string GetMessageText() const override;
  std::u16string GetActionButtonText() const override;
  base::TimeDelta GetDuration() const override;
  content::WebContents* GetWebContents() const override;

 private:
  // Dismisses the snackbar if it is showing. Calling Dismiss without calling
  // Show is no-op.
  void Dismiss();

  // Map the snackbar type to the corresponding UMA variant name for histogram.
  std::string GetSnackbarTypeForLogging();

  raw_ptr<content::WebContents> web_contents_;

  raw_ptr<AutofillSnackbarView> autofill_snackbar_view_ = nullptr;

  // The type of the progress dialog that is being displayed.
  AutofillSnackbarType autofill_snackbar_type_ =
      AutofillSnackbarType::kUnspecified;

  // The duration for which the snackbar should be shown before being dismissed.
  base::TimeDelta autofill_snackbar_duration_ = kDefaultSnackbarDuration;

  // Callback to run after the snackbar is dismissed.
  std::optional<base::OnceClosure> on_dismiss_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_SNACKBAR_CONTROLLER_IMPL_H_
