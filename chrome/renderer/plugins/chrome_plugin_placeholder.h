// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_

#include <stdint.h>
#include <string>

#include "chrome/common/buildflags.h"
#include "chrome/common/plugin.mojom.h"
#include "components/no_state_prefetch/renderer/prerender_observer.h"
#include "components/plugins/renderer/loadable_plugin_placeholder.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

class ChromePluginPlaceholder final
    : public plugins::LoadablePluginPlaceholder,
      public content::RenderThreadObserver,
      public blink::mojom::ContextMenuClient,
      public prerender::PrerenderObserver,
      public gin::Wrappable<ChromePluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static ChromePluginPlaceholder* CreateBlockedPlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params,
      const content::WebPluginInfo& info,
      const std::string& identifier,
      const std::u16string& name,
      int resource_id,
      const std::u16string& message);

  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static ChromePluginPlaceholder* CreateLoadableMissingPlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);

  ChromePluginPlaceholder(const ChromePluginPlaceholder&) = delete;
  ChromePluginPlaceholder& operator=(const ChromePluginPlaceholder&) = delete;

  // Runs |callback| over each plugin placeholder for the given RenderFrame.
  static void ForEach(
      content::RenderFrame* render_frame,
      const base::RepeatingCallback<void(ChromePluginPlaceholder*)>& callback);

  void SetStatus(chrome::mojom::PluginStatus status);

 private:
  ChromePluginPlaceholder(content::RenderFrame* render_frame,
                          const blink::WebPluginParams& params,
                          const std::u16string& title);
  ~ChromePluginPlaceholder() override;

  // content::LoadablePluginPlaceholder overrides.
  blink::WebPlugin* CreatePlugin() override;

  // gin::Wrappable (via PluginPlaceholder) method
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // WebViewPlugin::Delegate (via PluginPlaceholder) methods:
  v8::Local<v8::Value> GetV8Handle(v8::Isolate* isolate) override;
  void ShowContextMenu(const blink::WebMouseEvent&) override;

  // content::RenderThreadObserver methods:
  void PluginListChanged() override;

  // blink::mojom::ContextMenuClient methods.
  void CustomContextMenuAction(uint32_t action) override;
  void ContextMenuClosed(const GURL& link_followed) override;

  // prerender::PrerenderObserver methods:
  void SetIsPrerendering(bool is_prerendering) override;

  chrome::mojom::PluginStatus status_;

  std::u16string title_;

  std::u16string plugin_name_;

  mojo::AssociatedReceiver<blink::mojom::ContextMenuClient>
      context_menu_client_receiver_{this};
};

#endif  // CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
