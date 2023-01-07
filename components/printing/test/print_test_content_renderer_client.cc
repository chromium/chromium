// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/test/print_test_content_renderer_client.h"

#include <memory>

#include "components/printing/renderer/print_render_frame_helper.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/web/web_element.h"

namespace printing {

namespace {

class PrintRenderFrameHelperDelegate : public PrintRenderFrameHelper::Delegate {
 public:
  explicit PrintRenderFrameHelperDelegate(bool generate_tagged_pdfs)
      : generate_tagged_pdfs_(generate_tagged_pdfs) {}
  ~PrintRenderFrameHelperDelegate() override = default;

  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override {
    return blink::WebElement();
  }
  bool IsPrintPreviewEnabled() override {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    return true;
#else
    return false;
#endif
  }
  bool ShouldGenerateTaggedPDF() override { return generate_tagged_pdfs_; }
  bool OverridePrint(blink::WebLocalFrame* frame) override { return false; }

 private:
  const bool generate_tagged_pdfs_;
};

}  // namespace

PrintTestContentRendererClient::PrintTestContentRendererClient(
    bool generate_tagged_pdfs)
    : generate_tagged_pdfs_(generate_tagged_pdfs) {}

PrintTestContentRendererClient::~PrintTestContentRendererClient() = default;

void PrintTestContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new PrintRenderFrameHelper(
      render_frame,
      std::make_unique<PrintRenderFrameHelperDelegate>(generate_tagged_pdfs_));
}

}  // namespace printing
