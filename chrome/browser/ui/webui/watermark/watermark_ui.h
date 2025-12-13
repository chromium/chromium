// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_UI_H_

#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/browser/ui/webui/watermark/watermark.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

// Forward declaration
class WatermarkPageHandler;

// The WebUI for chrome://watermark
class WatermarkUI : public ui::MojoWebUIController,
                    public watermark::mojom::PageHandlerFactory {
 public:
  explicit WatermarkUI(content::WebUI* web_ui);
  ~WatermarkUI() override;

  // ui::MojoWebUIController
  void BindInterface(
      mojo::PendingReceiver<watermark::mojom::PageHandlerFactory> receiver);

  WatermarkPageHandler* GetPageHandlerForTesting() {
    return page_handler_.get();
  }

 private:
  // watermark::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingReceiver<watermark::mojom::PageHandler> receiver) override;

  std::unique_ptr<WatermarkPageHandler> page_handler_;
  mojo::Receiver<watermark::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://watermark
class WatermarkUIConfig : public content::DefaultWebUIConfig<WatermarkUI> {
 public:
  WatermarkUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_UI_H_
