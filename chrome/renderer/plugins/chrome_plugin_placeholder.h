// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/plugin.mojom.h"
#include "chrome/common/prerender_types.h"
#include "chrome/renderer/plugins/power_saver_info.h"
#include "components/plugins/renderer/loadable_plugin_placeholder.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ChromePluginPlaceholder final
    : public plugins::LoadablePluginPlaceholder,
      public content::RenderThreadObserver,
      public content::ContextMenuClient,
      public chrome::mojom::PluginRenderer,
      public gin::Wrappable<ChromePluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static ChromePluginPlaceholder* CreateBlockedPlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params,
      const content::WebPluginInfo& info,
      const std::string& identifier,
      const base::string16& name,
      int resource_id,
      const base::string16& message,
      const PowerSaverInfo& power_saver_info);

  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static ChromePluginPlaceholder* CreateLoadableMissingPlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);

  void SetStatus(chrome::mojom::PluginStatus status);

  mojo::PendingRemote<chrome::mojom::PluginRenderer> BindPluginRenderer();

 private:
  ChromePluginPlaceholder(content::RenderFrame* render_frame,
                          const blink::WebPluginParams& params,
                          const std::string& html_data,
                          const base::string16& title);
  ~ChromePluginPlaceholder() override;

  // content::LoadablePluginPlaceholder overrides.
  blink::WebPlugin* CreatePlugin() override;
  void OnBlockedContent(content::RenderFrame::PeripheralContentStatus status,
                        bool is_same_origin) override;

  // gin::Wrappable (via PluginPlaceholder) method
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // content::RenderViewObserver (via PluginPlaceholder) override:
  bool OnMessageReceived(const IPC::Message& message) override;

  // WebViewPlugin::Delegate (via PluginPlaceholder) methods:
  v8::Local<v8::Value> GetV8Handle(v8::Isolate* isolate) override;
  void ShowContextMenu(const blink::WebMouseEvent&) override;

  // content::RenderThreadObserver methods:
  void PluginListChanged() override;

  // content::ContextMenuClient methods:
  void OnMenuAction(int request_id, unsigned action) override;
  void OnMenuClosed(int request_id) override;

  // Show the Plugins permission bubble.
  void ShowPermissionBubbleCallback();

  // chrome::mojom::PluginRenderer methods.
  void FinishedDownloading() override;
  void UpdateDownloading() override;
  void UpdateSuccess() override;
  void UpdateFailure() override;

  // IPC message handlers:
  void OnSetPrerenderMode(prerender::PrerenderMode mode,
                          const std::string& histogram_prefix);

  chrome::mojom::PluginStatus status_;

  base::string16 title_;

  int context_menu_request_id_;  // Nonzero when request pending.
  base::string16 plugin_name_;

  mojo::Receiver<chrome::mojom::PluginRenderer> plugin_renderer_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromePluginPlaceholder);
};

#endif  // CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
