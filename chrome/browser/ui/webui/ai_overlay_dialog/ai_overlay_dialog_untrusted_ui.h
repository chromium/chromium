// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_UNTRUSTED_UI_H_

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class AiOverlayDialogUntrustedUI;

class AiOverlayDialogUntrustedUIConfig
    : public content::DefaultWebUIConfig<AiOverlayDialogUntrustedUI> {
 public:
  AiOverlayDialogUntrustedUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class AiOverlayDialogUntrustedUI : public UntrustedTopChromeWebUIController {
 public:
  explicit AiOverlayDialogUntrustedUI(content::WebUI* web_ui);
  AiOverlayDialogUntrustedUI(const AiOverlayDialogUntrustedUI&) = delete;
  AiOverlayDialogUntrustedUI& operator=(const AiOverlayDialogUntrustedUI&) =
      delete;
  ~AiOverlayDialogUntrustedUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_UNTRUSTED_UI_H_
