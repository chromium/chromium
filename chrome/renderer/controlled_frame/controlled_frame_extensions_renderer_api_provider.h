// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CONTROLLED_FRAME_CONTROLLED_FRAME_EXTENSIONS_RENDERER_API_PROVIDER_H_
#define CHROME_RENDERER_CONTROLLED_FRAME_CONTROLLED_FRAME_EXTENSIONS_RENDERER_API_PROVIDER_H_

#include "extensions/renderer/extensions_renderer_api_provider.h"

namespace extensions {
class ResourceBundleSourceMap;
class ScriptContext;
}  // namespace extensions

namespace controlled_frame {

// Provides the Controlled Frame API JavaScript source files, custom element,
// and WebView modules to the extension system's renderer side.
class ControlledFrameExtensionsRendererAPIProvider
    : public extensions::ExtensionsRendererAPIProvider {
 public:
  ControlledFrameExtensionsRendererAPIProvider() = default;
  ~ControlledFrameExtensionsRendererAPIProvider() override = default;

  // extensions::ExtensionsRendererAPIProvider implementation:
  ControlledFrameExtensionsRendererAPIProvider(
      const ControlledFrameExtensionsRendererAPIProvider&) = delete;
  ControlledFrameExtensionsRendererAPIProvider& operator=(
      const ControlledFrameExtensionsRendererAPIProvider&) = delete;

  void EnableCustomElementAllowlist() override;
  void PopulateSourceMap(
      extensions::ResourceBundleSourceMap* source_map) override;
  bool RequireWebViewModules(extensions::ScriptContext* context) override;
};

}  // namespace controlled_frame

#endif  // CHROME_RENDERER_CONTROLLED_FRAME_CONTROLLED_FRAME_EXTENSIONS_RENDERER_API_PROVIDER_H_
