// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SCANNER_FEEDBACK_DIALOG_SCANNER_FEEDBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SCANNER_FEEDBACK_DIALOG_SCANNER_FEEDBACK_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

namespace ash {

// Dialog delegate for the Scanner feedback form WebUI.
//
// To show this dialog, construct a new unowned instance of this class using
// `new` and call `ShowSystemDialog[ForBrowserContext]`. The class instance will
// be destroyed when the dialog is closed.
class ScannerFeedbackDialog : public SystemWebDialogDelegate {
 public:
  ScannerFeedbackDialog();

  ScannerFeedbackDialog(const ScannerFeedbackDialog&) = delete;
  ScannerFeedbackDialog& operator=(const ScannerFeedbackDialog&) = delete;

  ~ScannerFeedbackDialog() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SCANNER_FEEDBACK_DIALOG_SCANNER_FEEDBACK_DIALOG_H_
