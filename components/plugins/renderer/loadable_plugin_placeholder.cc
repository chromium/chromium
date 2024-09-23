// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/loadable_plugin_placeholder.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/string_escape.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::UserMetricsAction;
using content::RenderFrame;
using content::RenderThread;

namespace plugins {

void LoadablePluginPlaceholder::MaybeLoadBlockedPlugin(
    const std::string& identifier) {
  if (!identifier.empty() && identifier != identifier_)
    return;

  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_UI"));
  LoadPlugin();
}

LoadablePluginPlaceholder::LoadablePluginPlaceholder(
    RenderFrame* render_frame,
    const blink::WebPluginParams& params)
    : PluginPlaceholderBase(render_frame, params) {}

LoadablePluginPlaceholder::~LoadablePluginPlaceholder() {
}

void LoadablePluginPlaceholder::ReplacePlugin(blink::WebPlugin* new_plugin) {
  CHECK(plugin());
  if (!new_plugin)
    return;
  blink::WebPluginContainer* container = plugin()->Container();
  // This can occur if the container has been destroyed.
  if (!container) {
    new_plugin->Destroy();
    return;
  }

  container->SetPlugin(new_plugin);
  if (!new_plugin->Initialize(container)) {
    if (new_plugin->Container()) {
      // Since the we couldn't initialize the new plugin, but the container
      // still exists, restore the placeholder and destroy the new plugin.
      container->SetPlugin(plugin());
      new_plugin->Destroy();
    } else {
      // The container has been destroyed, along with the new plugin. Destroy
      // our placeholder plugin also.
      plugin()->Destroy();
    }
    return;
  }

  // During initialization, the new plugin might have replaced itself in turn
  // with another plugin. Make sure not to use the passed in |new_plugin| after
  // this point.
  new_plugin = container->Plugin();

  container->Invalidate();
  container->ReportGeometry();
  container->GetElement().SetAttribute("title", plugin()->old_title());
  plugin()->ReplayReceivedData(new_plugin);
  plugin()->Destroy();
}

void LoadablePluginPlaceholder::UpdateMessage() {
  if (!plugin())
    return;
  std::string script =
      "window.setMessage(" + base::GetQuotedJSONString(message_) + ")";
  plugin()->main_frame()->ExecuteScript(
      blink::WebScriptSource(blink::WebString::FromUTF8(script)));
}

bool LoadablePluginPlaceholder::IsErrorPlaceholder() {
  return !allow_loading_;
}

void LoadablePluginPlaceholder::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first navigation,
  // so no BlockedPlugin should see the notification that enables prerendering.
  DCHECK(!is_prerendering);
  if (is_blocked_for_prerendering_) {
    is_blocked_for_prerendering_ = false;
    if (!LoadingBlocked())
      LoadPlugin();
  }
}

void LoadablePluginPlaceholder::LoadPlugin() {
  // This is not strictly necessary but is an important defense in case the
  // event propagation changes between "close" vs. "click-to-play".
  if (hidden())
    return;
  if (!plugin())
    return;
  if (!allow_loading_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  ReplacePlugin(CreatePlugin());
}

void LoadablePluginPlaceholder::LoadCallback() {
  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Click"));
  // If the user specifically clicks on the plugin content's placeholder,
  // disable power saver throttling for this instance.
  LoadPlugin();
}

void LoadablePluginPlaceholder::DidFinishLoadingCallback() {
  finished_loading_ = true;
  if (message_.length() > 0)
    UpdateMessage();

  // In case our initial geometry was reported before the placeholder finished
  // loading, request another one. Needed for correct large poster unthrottling.
  if (plugin()) {
    CHECK(plugin()->Container());
    plugin()->Container()->ReportGeometry();
  }
}

void LoadablePluginPlaceholder::SetPluginInfo(
    const content::WebPluginInfo& plugin_info) {
  plugin_info_ = plugin_info;
}

const content::WebPluginInfo& LoadablePluginPlaceholder::GetPluginInfo() const {
  return plugin_info_;
}

void LoadablePluginPlaceholder::SetIdentifier(const std::string& identifier) {
  identifier_ = identifier;
}

bool LoadablePluginPlaceholder::LoadingBlocked() const {
  DCHECK(allow_loading_);
  return is_blocked_for_prerendering_;
}

}  // namespace plugins
