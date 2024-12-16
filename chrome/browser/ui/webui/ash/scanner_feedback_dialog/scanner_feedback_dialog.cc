// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/scanner_feedback_dialog/scanner_feedback_dialog.h"

#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

ScannerFeedbackDialog::ScannerFeedbackDialog()
    : SystemWebDialogDelegate(GURL(kScannerFeedbackUntrustedUrl),
                              /*title=*/u"") {
  set_show_close_button(false);
  // Taken from orca-feedback.ts's `IDEAL_WIDTH` and `IDEAL_HEIGHT`.
  set_dialog_size(gfx::Size(/*width=*/512, /*height=*/600));
}

ScannerFeedbackDialog::~ScannerFeedbackDialog() = default;

}  // namespace ash
