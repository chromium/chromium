// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MODAL_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MODAL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {

class WebContents;

}  //  namespace content

namespace safe_browsing {

// A tab modal dialog that is shown when the user's tailored security bit
// changes and the user isn't consented to sync.
// TODO(crbug.com/40847463): Remove this modal after launching
// `TailoredSecurityDesktopModal`.
class TailoredSecurityUnconsentedModal : public views::DialogDelegateView {
  METADATA_HEADER(TailoredSecurityUnconsentedModal, views::DialogDelegateView)

 public:
  // Show this dialog for the given |web_contents|.
  static void ShowForWebContents(content::WebContents* web_contents);

  explicit TailoredSecurityUnconsentedModal(content::WebContents* web_contents);
  TailoredSecurityUnconsentedModal(const TailoredSecurityUnconsentedModal&) =
      delete;
  TailoredSecurityUnconsentedModal& operator=(
      const TailoredSecurityUnconsentedModal&) = delete;
  ~TailoredSecurityUnconsentedModal() override;

  // views::DialogDelegate implementation:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;

 private:
  // views::DialogDelegateView:
  void AddedToWidget() override;

  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MODAL_H_
