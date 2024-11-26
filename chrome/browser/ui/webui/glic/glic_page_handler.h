// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_GLIC_GLIC_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_GLIC_GLIC_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}
namespace gfx {
class Size;
}  // namespace gfx

namespace glic {
class GlicWebClientHandler;

// Handles the Mojo requests coming from the Glic WebUI.
class GlicPageHandler : public glic::mojom::PageHandler {
 public:
  GlicPageHandler(content::BrowserContext* browser_context,
                  mojo::PendingReceiver<glic::mojom::PageHandler> receiver);

  GlicPageHandler(const GlicPageHandler&) = delete;
  GlicPageHandler& operator=(const GlicPageHandler&) = delete;

  ~GlicPageHandler() override;

  void CreateWebClient(::mojo::PendingReceiver<glic::mojom::WebClientHandler>
                           web_client_receiver) override;

 private:
  // There should at most one WebClientHandler at a time. A new one is created
  // each time the webview loads a page.
  std::unique_ptr<GlicWebClientHandler> web_client_handler_;
  raw_ptr<content::BrowserContext> browser_context_;
  mojo::Receiver<glic::mojom::PageHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  base::WeakPtrFactory<GlicPageHandler> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_UI_WEBUI_GLIC_GLIC_PAGE_HANDLER_H_
