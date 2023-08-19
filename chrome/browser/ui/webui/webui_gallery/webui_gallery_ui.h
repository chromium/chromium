// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class WebUI;
}

namespace ui {
class ColorChangeHandler;
}

// The Web UI controller for the chrome://webui-gallery page.
class WebuiGalleryUI : public ui::MojoWebUIController {
 public:
  explicit WebuiGalleryUI(content::WebUI* web_ui);
  ~WebuiGalleryUI() override;

  WebuiGalleryUI(const WebuiGalleryUI&) = delete;
  WebuiGalleryUI& operator=(const WebuiGalleryUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_
