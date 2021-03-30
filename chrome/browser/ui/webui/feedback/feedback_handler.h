// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_HANDLER_H_

#include <memory>

#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

class FeedbackHandler : public content::WebUIMessageHandler {
 public:
  explicit FeedbackHandler(const FeedbackDialog* dialog);
  FeedbackHandler(const FeedbackHandler&) = delete;
  FeedbackHandler& operator=(const FeedbackHandler&) = delete;
  ~FeedbackHandler() override;

  // Overrides from content::WebUIMessageHandler
  void RegisterMessages() override;

 private:
  // JS message handler for chrome.send("showDialog")
  void HandleShowDialog(const base::ListValue* args);

  const FeedbackDialog* dialog_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_HANDLER_H_
