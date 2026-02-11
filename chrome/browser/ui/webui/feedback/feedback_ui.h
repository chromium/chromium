// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"

// The implementation for the chrome://feedback page.
class FeedbackUI : public ui::WebDialogUI {
 public:
  explicit FeedbackUI(content::WebUI* web_ui);
  FeedbackUI(const FeedbackUI&) = delete;
  FeedbackUI& operator=(const FeedbackUI&) = delete;
  ~FeedbackUI() override;

  static bool IsFeedbackEnabled(Profile* profile);
  static constexpr std::string_view GetWebUIName() { return "Feedback"; }

  void set_embedder(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder) {
    embedder_ = embedder;
  }
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder() {
    return embedder_;
  }

 private:
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://feedback
class FeedbackUIConfig : public DefaultTopChromeWebUIConfig<FeedbackUI> {
 public:
  FeedbackUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  bool ShouldAutoResizeHost() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_
