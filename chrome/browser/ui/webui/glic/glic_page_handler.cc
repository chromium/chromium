// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/glic/glic_page_handler.h"

#include "base/version_info/version_info.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace glic {
class GlicWebClientHandler : public glic::mojom::WebClientHandler {
 public:
  explicit GlicWebClientHandler(
      GlicKeyedService* glic_service,
      mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver)
      : glic_service_(glic_service), receiver_(this, std::move(receiver)) {}

  // glic::mojom::WebClientHandler implementation.
  void WebClientInitialized(
      ::mojo::PendingRemote<glic::mojom::WebClient> web_client) override {
    web_client_.Bind(std::move(web_client));
  }

  void GetChromeVersion(GetChromeVersionCallback callback) override {
    std::move(callback).Run(version_info::GetVersion());
  }

  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t> window_id,
                 CreateTabCallback callback) override {
    glic_service_->CreateTab(url, open_in_background, window_id,
                             std::move(callback));
  }

  void ClosePanel() override { glic_service_->ClosePanel(); }

  void ResizeWidget(const gfx::Size& size,
                    ResizeWidgetCallback callback) override {
    std::optional<gfx::Size> actual_size = glic_service_->ResizePanel(size);
    if (!actual_size) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(actual_size);
  }

  void GetContextFromFocusedTab(
      bool include_inner_text,
      bool include_viewport_screenshot,
      GetContextFromFocusedTabCallback callback) override {
    glic_service_->GetContextFromFocusedTab(
        include_inner_text, include_viewport_screenshot, std::move(callback));
  }

 private:
  raw_ptr<GlicKeyedService> glic_service_;
  mojo::Receiver<glic::mojom::WebClientHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
};

GlicPageHandler::GlicPageHandler(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver)
    : browser_context_(browser_context), receiver_(this, std::move(receiver)) {}

GlicPageHandler::~GlicPageHandler() = default;

void GlicPageHandler::CreateWebClient(
    ::mojo::PendingReceiver<glic::mojom::WebClientHandler>
        web_client_receiver) {
  web_client_handler_ = std::make_unique<GlicWebClientHandler>(
      GlicKeyedServiceFactory::GetGlicKeyedService(browser_context_),
      std::move(web_client_receiver));
}

}  // namespace glic
