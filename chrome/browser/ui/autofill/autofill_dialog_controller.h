// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#include <string>

#include "content/public/browser/web_contents.h"

namespace autofill {

// Controller interface that exposes dialog functionality to autofill views.
class AutofillDialogController {
 public:
  virtual ~AutofillDialogController() = default;

  virtual void Show(const std::u16string& title,
                    const std::u16string& description,
                    const std::u16string& button_text,
                    base::OnceClosure on_positive_button_clicked_callback) = 0;

  // User clicked the positive button on the dialog.
  virtual void OnPositiveButtonClicked() = 0;

  // The dialog was dismissed without any user interaction.
  virtual void OnDismissed() = 0;

  // Returns the text to be displayed in the title area of the dialog.
  virtual std::u16string GetTitleText() const = 0;
  // Returns the text to be displayed in the description area of the dialog.
  virtual std::u16string GetDescriptionText() const = 0;
  // Returns the text to be displayed in the button of the dialog.
  virtual std::u16string GetButtonText() const = 0;
  virtual content::WebContents& GetWebContents() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
