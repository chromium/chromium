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

void ControlledFrameExtensionsRendererAPIProvider::RegisterNativeHandlers(
    extensions::ModuleSystem* module_system,
    extensions::NativeExtensionBindingsSystem* bindings_system,
    extensions::V8SchemaRegistry* v8_schema_registry,
    extensions::ScriptContext* context) const {}

void ControlledFrameExtensionsRendererAPIProvider::AddBindingsSystemHooks(
    extensions::Dispatcher* dispatcher,
    extensions::NativeExtensionBindingsSystem* bindings_system) const {}

void ControlledFrameExtensionsRendererAPIProvider::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) const {
  source_map->RegisterSource("controlledFrame", IDR_CONTROLLED_FRAME_JS);
  source_map->RegisterSource("controlledFrameImpl",
                             IDR_CONTROLLED_FRAME_IMPL_JS);
  source_map->RegisterSource("controlledFrameInternal",
                             IDR_CONTROLLED_FRAME_INTERNAL_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("controlledFrameApiMethods",
                             IDR_CONTROLLED_FRAME_API_METHODS_JS);
}

void ControlledFrameExtensionsRendererAPIProvider::
    EnableCustomElementAllowlist() const {
  blink::WebCustomElement::AddEmbedderCustomElementName("controlledframe");
}

void ControlledFrameExtensionsRendererAPIProvider::RequireWebViewModules(
    extensions::ScriptContext* context) const {
  if (context->GetAvailability("controlledFrameInternal").is_available()) {
    // CHECK chromeWebViewInternal since controlledFrame will be built on top
    // of it.
    CHECK(context->GetAvailability("chromeWebViewInternal").is_available());
    context->module_system()->Require("controlledFrame");
  }
}

}  // namespace controlled_frame
