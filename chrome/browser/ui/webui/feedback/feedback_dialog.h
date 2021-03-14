// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_DIALOG_H_

#include <string>
#include <vector>

#include "ui/web_dialogs/web_dialog_delegate.h"

namespace views {
class Widget;
}

namespace extensions {
namespace api {
namespace feedback_private {
struct FeedbackInfo;
}
}  // namespace api
}  // namespace extensions

class FeedbackDialog : public ui::WebDialogDelegate {
 public:
  static void CreateOrShow(
      const extensions::api::feedback_private::FeedbackInfo& info);

  FeedbackDialog(const FeedbackDialog&) = delete;
  FeedbackDialog& operator=(const FeedbackDialog&) = delete;
  ~FeedbackDialog() override;

 private:
  explicit FeedbackDialog(
      const extensions::api::feedback_private::FeedbackInfo& info);

  // Overrides from ui::WebDialogDelegate
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldShowCloseButton() const override;
  ui::WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;

  // Widget for the Feedback WebUI.
  views::Widget* widget_;
  static FeedbackDialog* current_instance_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_DIALOG_H_
