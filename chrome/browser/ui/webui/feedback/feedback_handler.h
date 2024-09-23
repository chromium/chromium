// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "content/public/browser/web_ui_message_handler.h"

class FeedbackHandler : public content::WebUIMessageHandler {
 public:
  explicit FeedbackHandler(base::WeakPtr<FeedbackDialog> dialog);
  FeedbackHandler(const FeedbackHandler&) = delete;
  FeedbackHandler& operator=(const FeedbackHandler&) = delete;
  ~FeedbackHandler() override;

  // Overrides from content::WebUIMessageHandler
  void RegisterMessages() override;

 private:
  void HandleShowDialog(const base::Value::List& args);
#if BUILDFLAG(IS_CHROMEOS)
  void HandleShowAssistantLogsInfo(const base::Value::List& args);
#endif  // BUILDFLAG(IS_CHROMEOS)
  void HandleShowAutofillMetadataInfo(const base::Value::List& args);
  void HandleShowMetrics(const base::Value::List& args);
  void HandleShowSystemInfo(const base::Value::List& args);

  base::WeakPtr<FeedbackDialog> dialog_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_HANDLER_H_
