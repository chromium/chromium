// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_view_web_plugin_client.h"

#include "base/check.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/render_thread.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/web/web_element.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/renderer/print_render_frame_helper.h"
#endif  // BUILDFLAG(ENABLE_PRINTING)

namespace pdf {

PdfViewWebPluginClient::PdfViewWebPluginClient(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  DCHECK(render_frame_);
}

PdfViewWebPluginClient::~PdfViewWebPluginClient() = default;

void PdfViewWebPluginClient::Print(const blink::WebElement& element) {
  DCHECK(!element.IsNull());
#if BUILDFLAG(ENABLE_PRINTING)
  printing::PrintRenderFrameHelper::Get(render_frame_)->PrintNode(element);
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

void PdfViewWebPluginClient::RecordComputedAction(const std::string& action) {
  content::RenderThread::Get()->RecordComputedAction(action);
}

std::unique_ptr<chrome_pdf::PdfAccessibilityDataHandler>
PdfViewWebPluginClient::CreateAccessibilityDataHandler(
    chrome_pdf::PdfAccessibilityActionHandler* action_handler) {
  return std::make_unique<PdfAccessibilityTree>(render_frame_, action_handler);
}

bool PdfViewWebPluginClient::IsUseZoomForDSFEnabled() const {
  return content::IsUseZoomForDSFEnabled();
}

}  // namespace pdf
