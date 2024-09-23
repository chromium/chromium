// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}  // namespace ui

// The implementation for the chrome://feedback page.
class FeedbackUI : public ui::WebDialogUI {
 public:
  explicit FeedbackUI(content::WebUI* web_ui);
  FeedbackUI(const FeedbackUI&) = delete;
  FeedbackUI& operator=(const FeedbackUI&) = delete;
  ~FeedbackUI() override;

  static bool IsFeedbackEnabled(Profile* profile);
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://feedback
class FeedbackUIConfig : public content::DefaultWebUIConfig<FeedbackUI> {
 public:
  FeedbackUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_
