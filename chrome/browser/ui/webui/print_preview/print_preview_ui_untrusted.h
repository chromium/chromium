// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_UNTRUSTED_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_UNTRUSTED_H_

#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace base {
class RefCountedMemory;
}  // namespace base

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
      content::WebUI* web_ui,
      const GURL& url) override;
};

class PrintPreviewUIUntrusted : public ui::UntrustedWebUIController {
 public:
  explicit PrintPreviewUIUntrusted(content::WebUI* web_ui);
  PrintPreviewUIUntrusted(const PrintPreviewUIUntrusted&) = delete;
  PrintPreviewUIUntrusted& operator=(const PrintPreviewUIUntrusted&) = delete;
  ~PrintPreviewUIUntrusted() override;

  static scoped_refptr<base::RefCountedMemory> GetPrintPreviewDataForTest(
      const std::string& path);

 private:
  // This is a member function to allow it to use `base::ScopedAllowBlocking`.
  static scoped_refptr<base::RefCountedMemory> GetPrintPreviewData(
      const std::string& path);

  // Member function in order to call `GetPrintPreviewData()` above.
  static void HandleRequestCallback(
      const std::string& path,
      content::WebUIDataSource::GotDataCallback callback);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_UNTRUSTED_H_
