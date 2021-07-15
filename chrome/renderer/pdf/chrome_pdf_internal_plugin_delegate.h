// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PDF_CHROME_PDF_INTERNAL_PLUGIN_DELEGATE_H_
#define CHROME_RENDERER_PDF_CHROME_PDF_INTERNAL_PLUGIN_DELEGATE_H_

#include "components/pdf/renderer/pdf_internal_plugin_delegate.h"

namespace content {
class RenderFrame;
}  // namespace content

class ChromePdfInternalPluginDelegate final
    : public pdf::PdfInternalPluginDelegate {
 public:
  explicit ChromePdfInternalPluginDelegate(content::RenderFrame* render_frame);
  ChromePdfInternalPluginDelegate(const ChromePdfInternalPluginDelegate&) =
      delete;
  ChromePdfInternalPluginDelegate& operator=(
      const ChromePdfInternalPluginDelegate&) = delete;
  ~ChromePdfInternalPluginDelegate() override;

  // `pdf::PdfInternalPluginDelegate`:
  bool IsAllowedFrame(const blink::WebFrame& frame) const override;
  std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient> CreatePrintClient()
      override;

 private:
  content::RenderFrame* render_frame_;
};

#endif  // CHROME_RENDERER_PDF_CHROME_PDF_INTERNAL_PLUGIN_DELEGATE_H_
