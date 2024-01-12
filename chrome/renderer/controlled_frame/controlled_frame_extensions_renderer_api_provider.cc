// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/controlled_frame/controlled_frame_extensions_renderer_api_provider.h"

#include "chrome/grit/renderer_resources.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_custom_element.h"

namespace controlled_frame {

void ControlledFrameExtensionsRendererAPIProvider::
    EnableCustomElementAllowlist() {
  blink::WebCustomElement::AddEmbedderCustomElementName("controlledframe");
}

void ControlledFrameExtensionsRendererAPIProvider::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {
  source_map->RegisterSource("controlledFrame", IDR_CONTROLLED_FRAME_JS);
  source_map->RegisterSource("controlledFrameImpl",
                             IDR_CONTROLLED_FRAME_IMPL_JS);
  source_map->RegisterSource("controlledFrameInternal",
                             IDR_CONTROLLED_FRAME_INTERNAL_CUSTOM_BINDINGS_JS);
}

bool ControlledFrameExtensionsRendererAPIProvider::RequireWebViewModules(
    extensions::ScriptContext* context) {
  if (context->GetAvailability("controlledFrameInternal").is_available()) {
    // CHECK chromeWebViewInternal since controlledFrame will be built on top
    // of it.
    CHECK(context->GetAvailability("chromeWebViewInternal").is_available());
    context->module_system()->Require("controlledFrame");
    return true;
  }
  return false;
}

}  // namespace controlled_frame
