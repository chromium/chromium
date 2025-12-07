// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_NON_LOADABLE_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_NON_LOADABLE_PLUGIN_PLACEHOLDER_H_

#include "content/public/common/buildflags.h"

namespace blink {
struct WebPluginParams;
}

namespace content {
class RenderFrame;
}

namespace plugins {
class PluginPlaceholder;
}

class NonLoadablePluginPlaceholder {
 public:
  NonLoadablePluginPlaceholder() = delete;
  NonLoadablePluginPlaceholder(const NonLoadablePluginPlaceholder&) = delete;
  NonLoadablePluginPlaceholder& operator=(const NonLoadablePluginPlaceholder&) =
      delete;

  // Creates a non-loadable plugin placeholder for platforms without plugins.
  static plugins::PluginPlaceholder* CreateNotSupportedPlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);

  static plugins::PluginPlaceholder* CreateFlashDeprecatedPlaceholder(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);
};

#endif  // CHROME_RENDERER_PLUGINS_NON_LOADABLE_PLUGIN_PLACEHOLDER_H_
