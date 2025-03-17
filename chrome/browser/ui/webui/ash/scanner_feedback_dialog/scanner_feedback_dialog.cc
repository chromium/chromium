// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/scanner_feedback_dialog/scanner_feedback_dialog.h"

#include <string>
#include <utility>
#include <variant>

#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_browser_context_data.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"
#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

ScannerFeedbackDialog::ScannerFeedbackDialog(
    ScannerFeedbackInfo info,
    ScannerDelegate::SendFeedbackCallback send_feedback_callback)
    : SystemWebDialogDelegate(GURL(kScannerFeedbackUntrustedUrl),
                              /*title=*/u""),
      feedback_info_(std::move(info)),
      send_feedback_callback_(std::move(send_feedback_callback)) {
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

  auto* feedback_info = std::get_if<ScannerFeedbackInfo>(&feedback_info_);
  // `OnDialogShown` should never be called multiple times. If it was previously
  // called, a UAF may occur after the previous dialog is closed - as that would
  // destroy `this` while the new `SystemWebDialogView` is still storing a (now
  // invalid) pointer to `this`.
  CHECK(feedback_info);
  CHECK(!send_feedback_callback_.is_null());

  CHECK(webui);
  content::WebContents* web_contents = webui->GetWebContents();
  CHECK(web_contents);
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  CHECK(browser_context);

  base::ScopedClosureRunner feedback_info_cleanup =
      SetScannerFeedbackInfoForBrowserContext(*browser_context,
                                              controller->page_handler().id(),
                                              std::move(*feedback_info));

  feedback_info_ = std::move(feedback_info_cleanup);

  ScannerFeedbackPageHandler& page_handler = controller->page_handler();
  page_handler.SetSendFeedbackCallback(std::move(send_feedback_callback_));

  // This is safe to run twice, as `Widget::Close()` explicitly handles the case
  // where a widget is attempted to be closed while it is already closed.
  page_handler.SetCloseDialogCallback(base::BindRepeating(
      &ScannerFeedbackDialog::Close, weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
