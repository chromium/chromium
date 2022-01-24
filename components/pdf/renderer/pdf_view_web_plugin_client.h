// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
#define COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_

#include "pdf/pdf_view_web_plugin.h"

namespace content {
class RenderFrame;
}

namespace pdf {

class PdfViewWebPluginClient : public chrome_pdf::PdfViewWebPlugin::Client {
 public:
  explicit PdfViewWebPluginClient(content::RenderFrame* render_frame);
  PdfViewWebPluginClient(const PdfViewWebPluginClient&) = delete;
  PdfViewWebPluginClient& operator=(const PdfViewWebPluginClient&) = delete;
  ~PdfViewWebPluginClient() override;

  // chrome_pdf::PdfViewWebPlugin::Client:
  void Print(const blink::WebElement& element) override;
  void RecordComputedAction(const std::string& action) override;
  std::unique_ptr<chrome_pdf::PdfAccessibilityDataHandler>
  CreateAccessibilityDataHandler(
      chrome_pdf::PdfAccessibilityActionHandler* action_handler) override;
  bool IsUseZoomForDSFEnabled() const override;

 private:
  content::RenderFrame* const render_frame_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
