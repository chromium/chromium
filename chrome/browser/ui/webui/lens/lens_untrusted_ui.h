// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LENS_LENS_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LENS_LENS_UNTRUSTED_UI_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/gfx/image/image.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace lens {

// WebUI controller for the chrome-untrusted://lens page.
class LensUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit LensUntrustedUI(content::WebUI* web_ui);

  LensUntrustedUI(const LensUntrustedUI&) = delete;
  LensUntrustedUI& operator=(const LensUntrustedUI&) = delete;
  ~LensUntrustedUI() override;

 private:
  void StartLoadScreenshot(
      const std::string& resource_path,
      content::WebUIDataSource::GotDataCallback got_data_callback);

  gfx::Image image_;

  base::WeakPtrFactory<LensUntrustedUI> weak_factory_{this};
};

}  // namespace lens
#endif  // CHROME_BROWSER_UI_WEBUI_LENS_LENS_UNTRUSTED_UI_H_
