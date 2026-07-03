// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter.mojom.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class VisualGuidedSetterUI;

namespace content {
class WebUI;
}

class VisualGuidedSetterUIConfig final
    : public content::DefaultWebUIConfig<VisualGuidedSetterUI> {
 public:
  // NOLINTNEXTLINE(modernize-use-equals-default)
  VisualGuidedSetterUIConfig()
      : DefaultWebUIConfig(
            content::kChromeUIScheme,
            chrome::kChromeUIDefaultBrowserVisualGuidedSetterHost) {}
};

class VisualGuidedSetterUI final
    : public ui::MojoWebUIController,
      public visual_guided_setter::mojom::PageHandlerFactory {
 public:
  explicit VisualGuidedSetterUI(content::WebUI* web_ui);
  VisualGuidedSetterUI(const VisualGuidedSetterUI&) = delete;
  VisualGuidedSetterUI& operator=(const VisualGuidedSetterUI&) = delete;
  ~VisualGuidedSetterUI() override;

  // Instantiates the implementor of the PageHandlerFactory mojo interface.
  void BindInterface(
      mojo::PendingReceiver<visual_guided_setter::mojom::PageHandlerFactory>
          receiver);

 private:
  // visual_guided_setter::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<visual_guided_setter::mojom::Page> page,
      mojo::PendingReceiver<visual_guided_setter::mojom::PageHandler> handler)
      override;

  std::unique_ptr<VisualGuidedSetterPageHandler> page_handler_;
  mojo::Receiver<visual_guided_setter::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_UI_H_
