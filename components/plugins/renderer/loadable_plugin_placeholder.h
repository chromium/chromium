// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"

namespace plugins {
// Placeholders can be used if a plugin is missing or not available
// (blocked or disabled).
class LoadablePluginPlaceholder : public PluginPlaceholderBase {
 public:
  void set_blocked_for_tinyness(bool blocked_for_tinyness) {
    is_blocked_for_tinyness_ = blocked_for_tinyness;
  }

  void set_blocked_for_background_tab(bool blocked_for_background_tab) {
    is_blocked_for_background_tab_ = blocked_for_background_tab;
  }

  void set_blocked_for_prerendering(bool blocked_for_prerendering) {
    is_blocked_for_prerendering_ = blocked_for_prerendering;
  }

  bool power_saver_enabled() const { return power_saver_enabled_; }

  void set_power_saver_enabled(bool power_saver_enabled) {
    power_saver_enabled_ = power_saver_enabled;
  }

  // Defer loading of plugin, and instead show the Power Saver poster image.
  void BlockForPowerSaverPoster();

  // When we load the plugin, use this already-created plugin, not a new one.
  void SetPremadePlugin(content::PluginInstanceThrottler* throttler);

  void AllowLoading() { allow_loading_ = true; }

 protected:
  LoadablePluginPlaceholder(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            const std::string& html_data);
  ~LoadablePluginPlaceholder() override;

  void MarkPluginEssential(
      content::PluginInstanceThrottler::PowerSaverUnthrottleMethod method);

  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetIsPrerendering(bool is_prerendering);

  void SetMessage(const base::string16& message);
  void SetPluginInfo(const content::WebPluginInfo& plugin_info);
  const content::WebPluginInfo& GetPluginInfo() const;
  void SetIdentifier(const std::string& identifier);
  const std::string& GetIdentifier() const;
  bool LoadingAllowed() const { return allow_loading_; }

  const gfx::Rect& unobscured_rect() { return unobscured_rect_; }

  // Replace this placeholder with a different plugin (which could be
  // a placeholder again).
  void ReplacePlugin(blink::WebPlugin* new_plugin);

  // Load the blocked plugin.
  void LoadPlugin();

  // Javascript callbacks:
  void LoadCallback();
  void DidFinishLoadingCallback();

  // True if the power saver heuristic has already been run on this content.
  bool heuristic_run_before_;

 private:
  // WebViewPlugin::Delegate methods:
  void PluginDestroyed() override;
  v8::Local<v8::Object> GetV8ScriptableObject(
      v8::Isolate* isolate) const override;
  void OnUnobscuredRectUpdate(const gfx::Rect& unobscured_rect) override;
  bool IsErrorPlaceholder() override;

  // RenderFrameObserver methods:
  void WasShown() override;

  void UpdateMessage();

  bool LoadingBlocked() const;

  // Plugin creation is embedder-specific.
  virtual blink::WebPlugin* CreatePlugin() = 0;

  // Embedder-specific behavior. This will only be called once per placeholder.
  virtual void OnBlockedContent(
      content::RenderFrame::PeripheralContentStatus status,
      bool is_same_origin) = 0;

  content::WebPluginInfo plugin_info_;

  base::string16 message_;

  // True if the plugin load was deferred because this might be a tiny plugin.
  // Plugin may be automatically loaded if it receives non-tiny geometry.
  bool is_blocked_for_tinyness_;

  // True if the plugin load was deferred due to page being a background tab.
  // Plugin may be automatically loaded when the page is foregrounded.
  bool is_blocked_for_background_tab_;

  // True if the plugin was blocked because the page was being prerendered.
  // Plugin may be automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;

  // True if the plugin load was deferred due to a Power Saver poster.
  bool is_blocked_for_power_saver_poster_;

  // True if power saver is enabled for this plugin and it has not been marked
  // essential (by a click or retroactive whitelisting).
  bool power_saver_enabled_;

  // When we load, uses this premade plugin instead of creating a new one.
  content::PluginInstanceThrottler* premade_throttler_;

  bool allow_loading_;

  bool finished_loading_;
  std::string identifier_;

  gfx::Rect unobscured_rect_;

  base::WeakPtrFactory<LoadablePluginPlaceholder> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoadablePluginPlaceholder);
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_
