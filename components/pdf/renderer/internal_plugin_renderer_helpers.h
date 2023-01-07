// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_INTERNAL_PLUGIN_RENDERER_HELPERS_H_
#define COMPONENTS_PDF_RENDERER_INTERNAL_PLUGIN_RENDERER_HELPERS_H_

#include <memory>

namespace blink {
class WebPlugin;
struct WebPluginParams;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace pdf {

class PdfInternalPluginDelegate;

// Returns `true` if the current process is a PDF renderer.
bool IsPdfRenderer();

// Tries to create an instance of the internal PDF plugin, returning `nullptr`
// if the plugin cannot be created. This function handles both the Pepper and
// Pepper-free implementations, delegating to `content::RenderFrame` when
// creating a Pepper plugin instance.
//
// Note that `blink::WebPlugin` has a special life cycle, so it's returned as a
// raw pointer here.
blink::WebPlugin* CreateInternalPlugin(
    blink::WebPluginParams params,
    content::RenderFrame* render_frame,
    std::unique_ptr<PdfInternalPluginDelegate> delegate);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_INTERNAL_PLUGIN_RENDERER_HELPERS_H_
