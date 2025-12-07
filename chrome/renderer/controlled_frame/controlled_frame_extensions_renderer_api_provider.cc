// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/controlled_frame/controlled_frame_extensions_renderer_api_provider.h"

#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/controlled_frame/web_url_pattern_natives.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_custom_element.h"

namespace controlled_frame {

void ControlledFrameExtensionsRendererAPIProvider::RegisterNativeHandlers(
    extensions::ModuleSystem* module_system,
    extensions::NativeExtensionBindingsSystem* bindings_system,
    extensions::V8SchemaRegistry* v8_schema_registry,
    extensions::ScriptContext* context) const {
  module_system->RegisterNativeHandler(
      "WebUrlPatternNatives",
      std::make_unique<controlled_frame::WebUrlPatternNatives>(context));
}

void ControlledFrameExtensionsRendererAPIProvider::AddBindingsSystemHooks(
    extensions::Dispatcher* dispatcher,
    extensions::NativeExtensionBindingsSystem* bindings_system) const {}

void ControlledFrameExtensionsRendererAPIProvider::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) const {
  struct RegisterSourceData {
    std::string_view name;
    int resource_id;
  };

  static constexpr RegisterSourceData kSources[] = {
      {"htmlControlledFrameElement", IDR_HTML_CONTROLLED_FRAME_ELEMENT_JS},
      {"controlledFrameApiMethods", IDR_CONTROLLED_FRAME_API_METHODS_JS},
      {"controlledFrameEvents", IDR_CONTROLLED_FRAME_EVENTS_JS},
      {"controlledFrameImpl", IDR_CONTROLLED_FRAME_IMPL_JS},
      {"controlledFrameInternal",
       IDR_CONTROLLED_FRAME_INTERNAL_CUSTOM_BINDINGS_JS},
      {"controlledFrameWebRequest", IDR_CONTROLLED_FRAME_WEB_REQUEST_JS},
      {"controlledFrameContextMenus", IDR_CONTROLLED_FRAME_CONTEXT_MENUS_JS},
      {"controlledFrameURLPatternsHelper",
       IDR_CONTROLLED_FRAME_URL_PATTERNS_HELPER_JS},
  };

  for (const auto& source : kSources) {
    source_map->RegisterSource(source.name, source.resource_id);
  }
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

    // CHECK that the Chrome WebView and Controlled Frame features aren't both
    // enabled in the same context. This is here because Controlled Frame
    // is based on WebView and modifies base classes in order to not ship some
    // APIs. These modifications could harm a live WebView instance if we
    // allowed both in a single instance, but these features aren't designed
    // to be enabled in the same instance. This check confirms that is held.
    CHECK(!context->GetAvailability("chromeWebViewTag").is_available());

    context->module_system()->Require("htmlControlledFrameElement");
  }
}

}  // namespace controlled_frame
