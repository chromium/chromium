// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pdf/chrome_pdf_view_web_plugin_print_client.h"

#include "base/check.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "third_party/blink/public/web/web_element.h"

ChromePdfViewWebPluginPrintClient::ChromePdfViewWebPluginPrintClient(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

ChromePdfViewWebPluginPrintClient::~ChromePdfViewWebPluginPrintClient() =
    default;

void ChromePdfViewWebPluginPrintClient::Print(
    const blink::WebElement& element) {
  DCHECK(!element.IsNull());
  printing::PrintRenderFrameHelper::Get(render_frame_)->PrintNode(element);
}
