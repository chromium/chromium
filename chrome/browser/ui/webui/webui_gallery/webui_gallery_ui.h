// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// The Web UI controller for the chrome://webui-gallery page.
class WebuiGalleryUI : public content::WebUIController {
 public:
  explicit WebuiGalleryUI(content::WebUI* web_ui);
  ~WebuiGalleryUI() override;

  WebuiGalleryUI(const WebuiGalleryUI&) = delete;
  WebuiGalleryUI& operator=(const WebuiGalleryUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_GALLERY_WEBUI_GALLERY_UI_H_
