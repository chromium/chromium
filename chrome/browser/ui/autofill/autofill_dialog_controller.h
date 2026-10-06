// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Controller interface that exposes dialog functionality to autofill views.
class AutofillDialogController {
 public:
  enum class Result {
    kAccepted,
    kDeclined,
    kUnknown,
  };

  using DialogResultCallback = base::OnceCallback<void(Result)>;

  virtual ~AutofillDialogController() = default;

  // Shows the dialog. The negative button is not displayed when an empty
  // negative button text is passed.
  virtual void Show(std::u16string title,
                    std::u16string description,
                    std::u16string positive_button_text,
                    std::u16string negative_button_text,
                    DialogResultCallback dialog_result_callback) = 0;

  virtual void ShowLoadingDialog(const std::u16string& title,
                                 base::TimeDelta min_time) = 0;

  // Dismisses the dialog if it is showing.
  virtual void Dismiss() = 0;

  // User clicked the positive button on the dialog.
  virtual void OnPositiveButtonClicked() = 0;

  // User clicked the negative button on the dialog.
  virtual void OnNegativeButtonClicked() = 0;

  // The dialog was dismissed without any user interaction.
  virtual void OnDismissed() = 0;

  // Returns the text to be displayed in the title area of the dialog.
  virtual std::u16string GetTitleText() const = 0;
  // Returns the text to be displayed in the description area of the dialog.
  virtual std::u16string GetDescriptionText() const = 0;
  // Returns the text to be displayed in the negative button of the dialog.
  virtual std::u16string GetNegativeButtonText() const = 0;
  // Returns the text to be displayed in the positive button of the dialog.
  virtual std::u16string GetPositiveButtonText() const = 0;
  virtual content::WebContents& GetWebContents() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
