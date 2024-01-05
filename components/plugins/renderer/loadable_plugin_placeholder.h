// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_thread.h"

namespace content {
class RenderFrame;
}

namespace plugins {

// Placeholders can be used if a plugin is missing or not available
// (blocked or disabled).
class LoadablePluginPlaceholder : public PluginPlaceholderBase {
 public:
  LoadablePluginPlaceholder(const LoadablePluginPlaceholder&) = delete;
  LoadablePluginPlaceholder& operator=(const LoadablePluginPlaceholder&) =
      delete;

  void set_blocked_for_prerendering(bool blocked_for_prerendering) {
    is_blocked_for_prerendering_ = blocked_for_prerendering;
  }

  void AllowLoading() { allow_loading_ = true; }

  // Load the blocked plugin if the identifier matches (or is empty).
  void MaybeLoadBlockedPlugin(const std::string& identifier);

 protected:
  LoadablePluginPlaceholder(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params);
  ~LoadablePluginPlaceholder() override;

  void OnSetIsPrerendering(bool is_prerendering);

  void SetPluginInfo(const content::WebPluginInfo& plugin_info);
  const content::WebPluginInfo& GetPluginInfo() const;
  void SetIdentifier(const std::string& identifier);
  bool LoadingAllowed() const { return allow_loading_; }

  // Replace this placeholder with a different plugin (which could be
  // a placeholder again).
  void ReplacePlugin(blink::WebPlugin* new_plugin);

  // Load the blocked plugin.
  void LoadPlugin();

  // Javascript callbacks:
  void LoadCallback();
  void DidFinishLoadingCallback();

 private:
  // WebViewPlugin::Delegate methods:
  bool IsErrorPlaceholder() override;

  void UpdateMessage();

  bool LoadingBlocked() const;

  // Plugin creation is embedder-specific.
  virtual blink::WebPlugin* CreatePlugin() = 0;

  content::WebPluginInfo plugin_info_;

  std::u16string message_;

  // True if the plugin was blocked because the page was being prerendered.
  // Plugin may be automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_ = false;

  bool allow_loading_ = false;

  bool finished_loading_ = false;
  std::string identifier_;

  base::WeakPtrFactory<LoadablePluginPlaceholder> weak_factory_{this};
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_
