// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_NON_LOADABLE_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_NON_LOADABLE_PLUGIN_PLACEHOLDER_H_

#include "base/macros.h"

namespace base {
class FilePath;
}

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
  // Creates a non-loadable plugin placeholder for platforms without plugins.
  static plugins::PluginPlaceholder* CreateNotSupportedPlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);

  static plugins::PluginPlaceholder* CreateFlashDeprecatedPlaceholder(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);

  static plugins::PluginPlaceholder* CreateErrorPlugin(
      content::RenderFrame* render_frame,
      const base::FilePath& file_path);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NonLoadablePluginPlaceholder);
};

#endif  // CHROME_RENDERER_PLUGINS_NON_LOADABLE_PLUGIN_PLACEHOLDER_H_
