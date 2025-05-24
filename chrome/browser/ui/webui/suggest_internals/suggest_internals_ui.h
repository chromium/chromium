// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_UI_H_

#include "build/build_config.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

class SuggestInternalsHandler;

class SuggestInternalsUI;

class SuggestInternalsUIConfig
    : public content::DefaultWebUIConfig<SuggestInternalsUI> {
 public:
  SuggestInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISuggestInternalsHost) {}
};

// The Web UI controller for the chrome://suggest-internals.
class SuggestInternalsUI : public ui::MojoWebUIController {
 public:
  explicit SuggestInternalsUI(content::WebUI* web_ui);
  SuggestInternalsUI(const SuggestInternalsUI&) = delete;
  SuggestInternalsUI& operator=(const SuggestInternalsUI&) = delete;
  ~SuggestInternalsUI() override;

  // Instantiates the implementor of the suggest_internals::mojom::PageHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<suggest_internals::mojom::PageHandler>
          pending_page_handler);

 private:
  std::unique_ptr<SuggestInternalsHandler> handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_UI_H_
