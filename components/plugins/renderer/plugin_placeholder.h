// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/plugins/renderer/webview_plugin.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace plugins {

// This abstract class is the base class of all plugin placeholders.
class PluginPlaceholderBase : public content::RenderFrameObserver,
                              public WebViewPlugin::Delegate {
 public:
  // |render_frame| is a weak pointer. If it is going away, our |plugin_| will
  // be destroyed as well and will notify us.
  PluginPlaceholderBase(content::RenderFrame* render_frame,
                        const blink::WebPluginParams& params,
                        const std::string& html_data);
  ~PluginPlaceholderBase() override;

  WebViewPlugin* plugin() { return plugin_; }

 protected:
  const blink::WebPluginParams& GetPluginParams() const;

  // WebViewPlugin::Delegate methods:
  void ShowContextMenu(const blink::WebMouseEvent&) override;
  void PluginDestroyed() override;
  v8::Local<v8::Object> GetV8ScriptableObject(
      v8::Isolate* isolate) const override;
  bool IsErrorPlaceholder() override;

  // Hide this placeholder.
  void HidePlugin();
  bool hidden() const { return hidden_; }

  // JavaScript callbacks:
  void HideCallback();
  void NotifyPlaceholderReadyForTestingCallback();

 private:
  // RenderFrameObserver methods:
  void OnDestruct() override;

  blink::WebPluginParams plugin_params_;
  WebViewPlugin* plugin_;

  bool hidden_;

  DISALLOW_COPY_AND_ASSIGN(PluginPlaceholderBase);
};

// A basic placeholder that supports only hiding.
class PluginPlaceholder final : public PluginPlaceholderBase,
                                public gin::Wrappable<PluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  PluginPlaceholder(content::RenderFrame* render_frame,
                    const blink::WebPluginParams& params,
                    const std::string& html_data);
  ~PluginPlaceholder() override;

 private:
  // WebViewPlugin::Delegate methods:
  v8::Local<v8::Value> GetV8Handle(v8::Isolate* isolate) final;

  // gin::Wrappable method:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
