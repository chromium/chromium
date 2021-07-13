// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PDF_CHROME_PDF_VIEW_WEB_PLUGIN_PRINT_CLIENT_H_
#define CHROME_RENDERER_PDF_CHROME_PDF_VIEW_WEB_PLUGIN_PRINT_CLIENT_H_

#include "pdf/pdf_view_web_plugin.h"

namespace blink {
class WebElement;
}

namespace content {
class RenderFrame;
}

class ChromePdfViewWebPluginPrintClient
    : public chrome_pdf::PdfViewWebPlugin::PrintClient {
 public:
  explicit ChromePdfViewWebPluginPrintClient(
      content::RenderFrame* render_frame);
  ChromePdfViewWebPluginPrintClient(const ChromePdfViewWebPluginPrintClient&) =
      delete;
  ChromePdfViewWebPluginPrintClient& operator=(
      const ChromePdfViewWebPluginPrintClient&) = delete;
  ~ChromePdfViewWebPluginPrintClient() override;

  // chrome_pdf::PdfViewWebPlugin:PrintClient:
  void Print(const blink::WebElement& element) override;

 private:
  content::RenderFrame* const render_frame_;
};

#endif  // CHROME_RENDERER_PDF_CHROME_PDF_VIEW_WEB_PLUGIN_PRINT_CLIENT_H_
