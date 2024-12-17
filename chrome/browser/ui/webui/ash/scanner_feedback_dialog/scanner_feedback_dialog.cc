// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/scanner_feedback_dialog/scanner_feedback_dialog.h"

#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"
#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

ScannerFeedbackDialog::ScannerFeedbackDialog(ScannerFeedbackInfo info)
    : SystemWebDialogDelegate(GURL(kScannerFeedbackUntrustedUrl),
                              /*title=*/u""),
      feedback_info_(std::move(info)) {
  set_show_close_button(false);
  // Taken from orca-feedback.ts's `IDEAL_WIDTH` and `IDEAL_HEIGHT`.
  set_dialog_size(gfx::Size(/*width=*/512, /*height=*/600));
}

ScannerFeedbackDialog::~ScannerFeedbackDialog() = default;

void ScannerFeedbackDialog::OnDialogShown(content::WebUI* webui) {
  // This is called from `ui::WebDialogUIBase::HandleRenderFrameCreated`, right
  // after the `content::RenderFrameHost` is created - before any JavaScript is
  // run.
  SystemWebDialogDelegate::OnDialogShown(webui);

  auto* controller =
      CHECK_DEREF(webui->GetController()).GetAs<ScannerFeedbackUntrustedUI>();

  CHECK(controller);

  // `OnDialogShown` should never be called multiple times. If it was previously
  // called, a UAF may occur after the previous dialog is closed - as that would
  // destroy `this` while the new `SystemWebDialogView` is still storing a (now
  // invalid) pointer to `this`.
  CHECK(feedback_info_.has_value());

  controller->page_handler().SetFeedbackInfo(std::move(*feedback_info_));
  // `feedback_info_` is currently a non-empty moved-from value. Explicitly
  // reset it to ensure the above `CHECK` works.
  feedback_info_.reset();
}

}  // namespace ash
