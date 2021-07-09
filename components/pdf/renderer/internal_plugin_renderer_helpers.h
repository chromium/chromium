// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_INTERNAL_PLUGIN_RENDERER_HELPERS_H_
#define COMPONENTS_PDF_RENDERER_INTERNAL_PLUGIN_RENDERER_HELPERS_H_

#include <memory>

#include "pdf/pdf_view_web_plugin.h"

namespace blink {
class WebPlugin;
struct WebPluginParams;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace pdf {

// Tries to create an instance of the internal PDF plugin, returning `nullptr`
// if the caller should create a Pepper plugin instance instead.
//
// `print_client` is optional, and may be `nullptr`.
//
// Note that `blink::WebPlugin` has a special life cycle, so it's returned as a
// raw pointer here.
blink::WebPlugin* MaybeCreateInternalPlugin(
    content::RenderFrame* render_frame,
    std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient> print_client,
    blink::WebPluginParams& params);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_INTERNAL_PLUGIN_RENDERER_HELPERS_H_
