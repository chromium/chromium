// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/loadable_plugin_placeholder.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/string_escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
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
using content::PluginInstanceThrottler;
using content::RenderFrame;
using content::RenderThread;

namespace plugins {

void LoadablePluginPlaceholder::BlockForPowerSaverPoster() {
  DCHECK(!is_blocked_for_power_saver_poster_);
  is_blocked_for_power_saver_poster_ = true;

  DCHECK(render_frame());
  render_frame()->RegisterPeripheralPlugin(
      url::Origin::Create(GURL(GetPluginParams().url)),
      base::Bind(&LoadablePluginPlaceholder::MarkPluginEssential,
                 weak_factory_.GetWeakPtr(),
                 PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_WHITELIST));
}

void LoadablePluginPlaceholder::SetPremadePlugin(
    content::PluginInstanceThrottler* throttler) {
  DCHECK(throttler);
  DCHECK(!premade_throttler_);
  heuristic_run_before_ = true;
  premade_throttler_ = throttler;
}

LoadablePluginPlaceholder::LoadablePluginPlaceholder(
    RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const std::string& html_data)
    : PluginPlaceholderBase(render_frame, params, html_data),
      heuristic_run_before_(false),
      is_blocked_for_tinyness_(false),
      is_blocked_for_background_tab_(false),
      is_blocked_for_prerendering_(false),
      is_blocked_for_power_saver_poster_(false),
      power_saver_enabled_(false),
      premade_throttler_(nullptr),
      allow_loading_(false),
      finished_loading_(false) {}

LoadablePluginPlaceholder::~LoadablePluginPlaceholder() {
}

void LoadablePluginPlaceholder::MarkPluginEssential(
    PluginInstanceThrottler::PowerSaverUnthrottleMethod method) {
  if (!power_saver_enabled_)
    return;

  power_saver_enabled_ = false;

  if (premade_throttler_)
    premade_throttler_->MarkPluginEssential(method);
  else if (method != PluginInstanceThrottler::UNTHROTTLE_METHOD_DO_NOT_RECORD)
    PluginInstanceThrottler::RecordUnthrottleMethodMetric(method);

  is_blocked_for_power_saver_poster_ = false;
  is_blocked_for_tinyness_ = false;
  if (!LoadingBlocked())
    LoadPlugin();
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
  bool plugin_needs_initialization =
      !premade_throttler_ || new_plugin != premade_throttler_->GetWebPlugin();
  if (plugin_needs_initialization && !new_plugin->Initialize(container)) {
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

void LoadablePluginPlaceholder::SetMessage(const base::string16& message) {
  message_ = message;
  if (finished_loading_)
    UpdateMessage();
}

void LoadablePluginPlaceholder::UpdateMessage() {
  if (!plugin())
    return;
  std::string script =
      "window.setMessage(" + base::GetQuotedJSONString(message_) + ")";
  plugin()->main_frame()->ExecuteScript(
      blink::WebScriptSource(blink::WebString::FromUTF8(script)));
}

void LoadablePluginPlaceholder::PluginDestroyed() {
  if (power_saver_enabled_) {
    if (premade_throttler_) {
      // Since the premade plugin has been detached from the container, it will
      // not be automatically destroyed along with the page.
      premade_throttler_->GetWebPlugin()->Destroy();
      premade_throttler_ = nullptr;
    } else if (is_blocked_for_power_saver_poster_) {
      // Record the NEVER unthrottle count only if there is no throttler.
      PluginInstanceThrottler::RecordUnthrottleMethodMetric(
          PluginInstanceThrottler::UNTHROTTLE_METHOD_NEVER);
    }

    // Prevent processing subsequent calls to MarkPluginEssential.
    power_saver_enabled_ = false;
  }

  PluginPlaceholderBase::PluginDestroyed();
}

v8::Local<v8::Object> LoadablePluginPlaceholder::GetV8ScriptableObject(
    v8::Isolate* isolate) const {
  // Pass through JavaScript access to the underlying throttled plugin.
  if (premade_throttler_ && premade_throttler_->GetWebPlugin()) {
    return premade_throttler_->GetWebPlugin()->V8ScriptableObject(isolate);
  }
  return v8::Local<v8::Object>();
}

bool LoadablePluginPlaceholder::IsErrorPlaceholder() {
  return !allow_loading_;
}

void LoadablePluginPlaceholder::OnUnobscuredRectUpdate(
    const gfx::Rect& unobscured_rect) {
  DCHECK(content::RenderThread::Get());
  if (!render_frame())
    return;

  if (!plugin() || !finished_loading_)
    return;

  if (!is_blocked_for_tinyness_ && !is_blocked_for_power_saver_poster_)
    return;

  if (unobscured_rect_ == unobscured_rect)
    return;

  unobscured_rect_ = unobscured_rect;

  float zoom_factor = plugin()->Container()->PageZoomFactor();
  int width = roundf(unobscured_rect_.width() / zoom_factor);
  int height = roundf(unobscured_rect_.height() / zoom_factor);
  int x = roundf(unobscured_rect_.x() / zoom_factor);
  int y = roundf(unobscured_rect_.y() / zoom_factor);

  // On a size update check if we now qualify as a essential plugin.
  url::Origin main_frame_origin =
      render_frame()->GetWebFrame()->Top()->GetSecurityOrigin();
  url::Origin content_origin = url::Origin::Create(GetPluginParams().url);
  RenderFrame::PeripheralContentStatus status =
      render_frame()->GetPeripheralContentStatus(
          main_frame_origin, content_origin, gfx::Size(width, height),
          heuristic_run_before_ ? RenderFrame::DONT_RECORD_DECISION
                                : RenderFrame::RECORD_DECISION);

  // Early exit for plugins that we've discovered to be essential.
  if (status != RenderFrame::CONTENT_STATUS_PERIPHERAL &&
      status != RenderFrame::CONTENT_STATUS_TINY) {
    MarkPluginEssential(
        heuristic_run_before_
            ? PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_SIZE_CHANGE
            : PluginInstanceThrottler::UNTHROTTLE_METHOD_DO_NOT_RECORD);

    if (!heuristic_run_before_ &&
        status == RenderFrame::CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_BIG) {
      render_frame()->WhitelistContentOrigin(content_origin);
    }

    return;
  }

  if (!heuristic_run_before_) {
    OnBlockedContent(status,
                     main_frame_origin.IsSameOriginWith(content_origin));
  }

  if (is_blocked_for_tinyness_ && status != RenderFrame::CONTENT_STATUS_TINY) {
    is_blocked_for_tinyness_ = false;
    if (!LoadingBlocked()) {
      LoadPlugin();
    }
  }

  if (is_blocked_for_power_saver_poster_) {
    // Adjust poster container padding and dimensions to center play button for
    // plugins and plugin posters that have their top or left portions obscured.
    std::string script = base::StringPrintf(
        "window.resizePoster('%dpx', '%dpx', '%dpx', '%dpx')", x, y, width,
        height);
    plugin()->main_frame()->ExecuteScript(
        blink::WebScriptSource(blink::WebString::FromUTF8(script)));
  }

  heuristic_run_before_ = true;
}

void LoadablePluginPlaceholder::WasShown() {
  if (is_blocked_for_background_tab_) {
    is_blocked_for_background_tab_ = false;
    if (!LoadingBlocked())
      LoadPlugin();
  }
}

void LoadablePluginPlaceholder::OnLoadBlockedPlugins(
    const std::string& identifier) {
  if (!identifier.empty() && identifier != identifier_)
    return;

  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_UI"));
  MarkPluginEssential(
      PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_OMNIBOX_ICON);
  LoadPlugin();
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
    NOTREACHED();
    return;
  }

  if (premade_throttler_) {
    premade_throttler_->SetHiddenForPlaceholder(false /* hidden */);
    ReplacePlugin(premade_throttler_->GetWebPlugin());
    premade_throttler_ = nullptr;
  } else {
    ReplacePlugin(CreatePlugin());
  }
}

void LoadablePluginPlaceholder::LoadCallback() {
  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Click"));
  // If the user specifically clicks on the plugin content's placeholder,
  // disable power saver throttling for this instance.
  MarkPluginEssential(PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_CLICK);
  LoadPlugin();
}

void LoadablePluginPlaceholder::DidFinishLoadingCallback() {
  finished_loading_ = true;
  if (message_.length() > 0)
    UpdateMessage();

  // Wait for the placeholder to finish loading to hide the premade plugin.
  // This is necessary to prevent a flicker.
  if (premade_throttler_ && power_saver_enabled_)
    premade_throttler_->SetHiddenForPlaceholder(true /* hidden */);

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

const std::string& LoadablePluginPlaceholder::GetIdentifier() const {
  return identifier_;
}

bool LoadablePluginPlaceholder::LoadingBlocked() const {
  DCHECK(allow_loading_);
  return is_blocked_for_tinyness_ || is_blocked_for_background_tab_ ||
         is_blocked_for_power_saver_poster_ || is_blocked_for_prerendering_;
}

}  // namespace plugins
