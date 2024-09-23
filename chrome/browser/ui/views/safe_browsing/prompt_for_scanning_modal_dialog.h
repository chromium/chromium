// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PROMPT_FOR_SCANNING_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PROMPT_FOR_SCANNING_MODAL_DIALOG_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// A tab modal dialog that provides more information to the user about the
// prompt for deep scanning.
class PromptForScanningModalDialog : public views::DialogDelegateView {
  METADATA_HEADER(PromptForScanningModalDialog, views::DialogDelegateView)

 public:
  // Show this dialog for the given |web_contents|.
  static void ShowForWebContents(content::WebContents* web_contents,
                                 const std::u16string& filename,
                                 base::OnceClosure accept_callback,
                                 base::OnceClosure open_now_callback);

  // Create a PromptForScanningModalDialog attached to |web_contents|. The
  // dialog will call |accept_callback| if the user accepts the prompt.
  PromptForScanningModalDialog(content::WebContents* web_contents,
                               const std::u16string& filename,
                               base::OnceClosure accept_callback,
                               base::OnceClosure open_now_callback);
  PromptForScanningModalDialog(const PromptForScanningModalDialog&) = delete;
  PromptForScanningModalDialog& operator=(const PromptForScanningModalDialog&) =
      delete;
  ~PromptForScanningModalDialog() override;

  // views::DialogDelegate implementation:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;

 private:
  base::OnceClosure open_now_callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PROMPT_FOR_SCANNING_MODAL_DIALOG_H_
