// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_DEEP_SCANNING_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_DEEP_SCANNING_MODAL_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// A tab modal dialog that prompts the user to confirm their intent to open the
// dialog while a scan is in progress.
class DeepScanningModalDialog : public TabModalConfirmDialogDelegate {
 public:
  // Create a DeepScanningModalDialog attached to |web_contents|. The
  // dialog will call |accept_callback| if the user accepts the prompt.
  DeepScanningModalDialog(content::WebContents* web_contents,
                          base::OnceClosure accept_callback);
  DeepScanningModalDialog(const DeepScanningModalDialog&) = delete;
  DeepScanningModalDialog& operator=(const DeepScanningModalDialog&) = delete;
  ~DeepScanningModalDialog() override;

 private:
  // TabModalConfirmDialogDelegate implementation.
  std::u16string GetTitle() override;
  std::u16string GetDialogMessage() override;
  std::u16string GetAcceptButtonTitle() override;
  std::u16string GetLinkText() const override;
  void OnLinkClicked(WindowOpenDisposition disposition) override;
  void OnAccepted() override;

  base::OnceClosure accept_callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_DEEP_SCANNING_MODAL_DIALOG_H_
