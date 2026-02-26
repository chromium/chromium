// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals.mojom.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

namespace content {
class BrowserContext;
}  // namespace content

class PrivateAiInternalsUI;
class PrivateAiInternalsPageHandler;

class PrivateAiInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<PrivateAiInternalsUI> {
 public:
  PrivateAiInternalsUIConfig();

  ~PrivateAiInternalsUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://private-ai-internals.
class PrivateAiInternalsUI : public ui::MojoWebUIController {
 public:
  explicit PrivateAiInternalsUI(content::WebUI* web_ui);
  ~PrivateAiInternalsUI() override;

  PrivateAiInternalsUI(const PrivateAiInternalsUI&) = delete;
  PrivateAiInternalsUI& operator=(const PrivateAiInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<
          private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver);

 private:
  std::unique_ptr<PrivateAiInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_UI_H_
