// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CRITICAL_ACTIONS_CRITICAL_ACTIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CRITICAL_ACTIONS_CRITICAL_ACTIONS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/critical_actions/critical_actions.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace critical_actions {

class CriticalActionsUI;

class CriticalActionsInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<CriticalActionsUI> {
 public:
  CriticalActionsInternalsUIConfig();
  ~CriticalActionsInternalsUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUIController for chrome://critical-actions-internals
class CriticalActionsUI : public ui::MojoWebUIController,
                          public mojom::PageHandlerFactory {
 public:
  explicit CriticalActionsUI(content::WebUI* web_ui);

  CriticalActionsUI(const CriticalActionsUI&) = delete;
  CriticalActionsUI& operator=(const CriticalActionsUI&) = delete;

  ~CriticalActionsUI() override;

  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

 private:
  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver) override;

  std::unique_ptr<mojom::PageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace critical_actions

#endif  // CHROME_BROWSER_UI_WEBUI_CRITICAL_ACTIONS_CRITICAL_ACTIONS_UI_H_
