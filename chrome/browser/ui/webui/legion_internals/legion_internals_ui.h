// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

namespace content {
class BrowserContext;
}  // namespace content

class LegionInternalsUI;
class LegionInternalsPageHandler;

class LegionInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<LegionInternalsUI> {
 public:
  LegionInternalsUIConfig();

  ~LegionInternalsUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://legion-internals.
class LegionInternalsUI : public ui::MojoWebUIController {
 public:
  explicit LegionInternalsUI(content::WebUI* web_ui);
  ~LegionInternalsUI() override;

  LegionInternalsUI(const LegionInternalsUI&) = delete;
  LegionInternalsUI& operator=(const LegionInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<legion_internals::mojom::LegionInternalsPageHandler>
          receiver);

 private:
  std::unique_ptr<LegionInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_UI_H_
