// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_view_web_plugin_client.h"

#include "base/check.h"
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

}  // namespace pdf
