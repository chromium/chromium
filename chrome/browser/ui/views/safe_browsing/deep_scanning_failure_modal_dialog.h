// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_DEEP_SCANNING_FAILURE_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_DEEP_SCANNING_FAILURE_MODAL_DIALOG_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// A tab modal dialog that provides more information to the user about the
// prompt for deep scanning.
class DeepScanningFailureModalDialog : public views::DialogDelegateView {
  METADATA_HEADER(DeepScanningFailureModalDialog, views::DialogDelegateView)

 public:
  // Show this dialog for the given |web_contents|.
  static void ShowForWebContents(content::WebContents* web_contents,
                                 base::OnceClosure accept_callback,
                                 base::OnceClosure cancel_callback,
                                 base::OnceClosure close_callback,
                                 base::OnceClosure open_now_callback);

  // Create a DeepScanningFailureModalDialog attached to |web_contents|. The
  // dialog will call |accept_callback| if the user accepts the prompt.
  DeepScanningFailureModalDialog(base::OnceClosure accept_callback,
                                 base::OnceClosure cancel_callback,
                                 base::OnceClosure close_callback,
                                 base::OnceClosure open_now_callback);
  DeepScanningFailureModalDialog(const DeepScanningFailureModalDialog&) =
      delete;
  DeepScanningFailureModalDialog& operator=(
      const DeepScanningFailureModalDialog&) = delete;
  ~DeepScanningFailureModalDialog() override;

  // views::DialogDelegate implementation:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;

 private:
  base::OnceClosure open_now_callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_DEEP_SCANNING_FAILURE_MODAL_DIALOG_H_
