// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_UNTRUSTED_UI_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_UNTRUSTED_UI_ANDROID_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace lens {

class LensOverlayUntrustedUIAndroidConfig : public content::WebUIConfig {
 public:
  LensOverlayUntrustedUIAndroidConfig();
  ~LensOverlayUntrustedUIAndroidConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

class LensOverlayUntrustedUIAndroid : public content::WebUIController {
 public:
  explicit LensOverlayUntrustedUIAndroid(content::WebUI* web_ui);
  ~LensOverlayUntrustedUIAndroid() override = default;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_UNTRUSTED_UI_ANDROID_H_
