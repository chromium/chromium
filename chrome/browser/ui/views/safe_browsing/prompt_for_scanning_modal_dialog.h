// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PROMPT_FOR_SCANNING_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PROMPT_FOR_SCANNING_MODAL_DIALOG_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/timer/timer.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// A tab modal dialog that provides more information to the user about the
// prompt for deep scanning.
class PromptForScanningModalDialog : public views::DialogDelegateView,
                                     public views::ButtonListener {
 public:
  // Show this dialog for the given |web_contents|.
  static void ShowForWebContents(content::WebContents* web_contents,
                                 const base::string16& filename,
                                 base::OnceClosure accept_callback,
                                 base::OnceClosure open_now_callback);

  // Create a PromptForScanningModalDialog attached to |web_contents|. The
  // dialog will call |accept_callback| if the user accepts the prompt.
  PromptForScanningModalDialog(content::WebContents* web_contents,
                               const base::string16& filename,
                               base::OnceClosure accept_callback,
                               base::OnceClosure open_now_callback);
  PromptForScanningModalDialog(const PromptForScanningModalDialog&) = delete;
  PromptForScanningModalDialog& operator=(const PromptForScanningModalDialog&) =
      delete;
  ~PromptForScanningModalDialog() override;

  // views::DialogDelegate implementation:
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate implementation:
  ui::ModalType GetModalType() const override;

  // views::ButtonListener implementation:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // The name of the file that this prompt was created for.
  base::string16 filename_;

  // The open now button for this dialog. The pointer is unowned, but this is a
  // child View of this dialog's View, so it has the same lifetime.
  views::Button* open_now_button_;

  // The callbacks to trigger on each way the dialog is resolved.
  base::OnceClosure open_now_callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PROMPT_FOR_SCANNING_MODAL_DIALOG_H_
