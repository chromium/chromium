// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

class WebuiGalleryUI;

class WebuiGalleryUIConfig
    : public content::DefaultInternalWebUIConfig<WebuiGalleryUI> {
 public:
  WebuiGalleryUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIWebuiGalleryHost) {}
};

// The Web UI controller for the chrome://webui-gallery page.
class WebuiGalleryUI : public ui::MojoWebUIController {
 public:
  explicit WebuiGalleryUI(content::WebUI* web_ui);
  ~WebuiGalleryUI() override;

  WebuiGalleryUI(const WebuiGalleryUI&) = delete;
  WebuiGalleryUI& operator=(const WebuiGalleryUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_
