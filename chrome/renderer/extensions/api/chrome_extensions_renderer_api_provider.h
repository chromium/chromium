// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_CHROME_EXTENSIONS_RENDERER_API_PROVIDER_H_
#define CHROME_RENDERER_EXTENSIONS_API_CHROME_EXTENSIONS_RENDERER_API_PROVIDER_H_

#include "extensions/renderer/extensions_renderer_api_provider.h"

namespace extensions {
class ResourceBundleSourceMap;
class ScriptContext;

// Provides capabilities for extension APIs defined at the //chrome layer.
class ChromeExtensionsRendererAPIProvider
    : public ExtensionsRendererAPIProvider {
 public:
  ChromeExtensionsRendererAPIProvider() = default;
  ChromeExtensionsRendererAPIProvider(
      const ChromeExtensionsRendererAPIProvider&) = delete;
  ChromeExtensionsRendererAPIProvider& operator=(
      const ChromeExtensionsRendererAPIProvider&) = delete;
  ~ChromeExtensionsRendererAPIProvider() override = default;

  // ExtensionsRendererAPIProvider:
  void RegisterNativeHandlers(ModuleSystem* module_system,
                              NativeExtensionBindingsSystem* bindings_system,
                              V8SchemaRegistry* v8_schema_registry,
                              ScriptContext* context) const override;
  void AddBindingsSystemHooks(
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system) const override;
  void PopulateSourceMap(ResourceBundleSourceMap* source_map) const override;
  void EnableCustomElementAllowlist() const override;
  void RequireWebViewModules(ScriptContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_CHROME_EXTENSIONS_RENDERER_API_PROVIDER_H_
