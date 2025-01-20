// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SCANNER_FEEDBACK_DIALOG_SCANNER_FEEDBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SCANNER_FEEDBACK_DIALOG_SCANNER_FEEDBACK_DIALOG_H_

#include <variant>

#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

namespace content {
class WebUI;
}

namespace ash {

// Dialog delegate for the Scanner feedback form WebUI.
//
// To show this dialog, construct a new unowned instance of this class using
// `new` and call `ShowSystemDialog[ForBrowserContext]`. The class instance will
// be destroyed when the dialog is closed.
class ScannerFeedbackDialog : public SystemWebDialogDelegate {
 public:
  explicit ScannerFeedbackDialog(
      ScannerFeedbackInfo info,
      ScannerDelegate::SendFeedbackCallback send_feedback_callback);

  ScannerFeedbackDialog(const ScannerFeedbackDialog&) = delete;
  ScannerFeedbackDialog& operator=(const ScannerFeedbackDialog&) = delete;

  ~ScannerFeedbackDialog() override;

  // SystemWebDialogDelegate:
  void OnDialogShown(content::WebUI* webui) override;

 private:
  // Set to a `ScannerFeedbackInfo` on construction.
  // `OnDialogShown` will move the `ScannerFeedbackInfo` into the WebUI's
  // browser context, and set this to `base::ScopedClosureRunner` to clean it
  // up once the dialog is closed.
  std::variant<ScannerFeedbackInfo, base::ScopedClosureRunner> feedback_info_;

  // Set on construction. Set to null on `OnDialogShown`.
  ScannerDelegate::SendFeedbackCallback send_feedback_callback_;

  base::WeakPtrFactory<ScannerFeedbackDialog> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SCANNER_FEEDBACK_DIALOG_SCANNER_FEEDBACK_DIALOG_H_
