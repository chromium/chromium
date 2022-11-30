// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_UNTRUSTED_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_UNTRUSTED_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace printing {

class PrintPreviewUIUntrustedConfig : public content::WebUIConfig {
 public:
  PrintPreviewUIUntrustedConfig();
  ~PrintPreviewUIUntrustedConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

class PrintPreviewUIUntrusted : public ui::UntrustedWebUIController {
 public:
  explicit PrintPreviewUIUntrusted(content::WebUI* web_ui);
  PrintPreviewUIUntrusted(const PrintPreviewUIUntrusted&) = delete;
  PrintPreviewUIUntrusted& operator=(const PrintPreviewUIUntrusted&) = delete;
  ~PrintPreviewUIUntrusted() override;
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_UNTRUSTED_H_
