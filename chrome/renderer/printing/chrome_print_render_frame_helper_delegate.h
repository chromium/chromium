// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINTING_CHROME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define CHROME_RENDERER_PRINTING_CHROME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "components/printing/renderer/print_render_frame_helper.h"

class ChromePrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  ChromePrintRenderFrameHelperDelegate();

  ChromePrintRenderFrameHelperDelegate(
      const ChromePrintRenderFrameHelperDelegate&) = delete;
  ChromePrintRenderFrameHelperDelegate& operator=(
      const ChromePrintRenderFrameHelperDelegate&) = delete;

  ~ChromePrintRenderFrameHelperDelegate() override;

 private:
  // printing::PrintRenderFrameHelper::Delegate:
  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;
  bool IsPrintPreviewEnabled() override;
  bool OverridePrint(blink::WebLocalFrame* frame) override;
  bool ShouldGenerateTaggedPDF() override;
};

#endif  // CHROME_RENDERER_PRINTING_CHROME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
