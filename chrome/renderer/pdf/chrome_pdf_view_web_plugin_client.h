// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PDF_CHROME_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
#define CHROME_RENDERER_PDF_CHROME_PDF_VIEW_WEB_PLUGIN_CLIENT_H_

#include "pdf/pdf_view_web_plugin.h"

namespace content {
class RenderFrame;
}

class ChromePdfViewWebPluginClient
    : public chrome_pdf::PdfViewWebPlugin::Client {
 public:
  explicit ChromePdfViewWebPluginClient(content::RenderFrame* render_frame);
  ChromePdfViewWebPluginClient(const ChromePdfViewWebPluginClient&) = delete;
  ChromePdfViewWebPluginClient& operator=(const ChromePdfViewWebPluginClient&) =
      delete;
  ~ChromePdfViewWebPluginClient() override;

  // chrome_pdf::PdfViewWebPlugin::Client:
  void Print(const blink::WebElement& element) override;

 private:
  content::RenderFrame* const render_frame_;
};

#endif  // CHROME_RENDERER_PDF_CHROME_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
