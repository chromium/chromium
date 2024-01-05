// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/plugin_placeholder.h"

#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/re2/src/re2/re2.h"

namespace plugins {

// The placeholder is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
const char kPluginPlaceholderDataURL[] = "data:text/html,pluginplaceholderdata";

PluginPlaceholderBase::PluginPlaceholderBase(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params)
    : content::RenderFrameObserver(render_frame), plugin_params_(params) {}

PluginPlaceholderBase::~PluginPlaceholderBase() = default;

void PluginPlaceholderBase::Init(const std::string& html_data) {
  CHECK(!plugin_);
  auto* frame = render_frame();
  // The `WebViewPlugin::Delegate` represented by `this` can get called during
  // the Create method, so this can't be in the constructor.
  plugin_ = WebViewPlugin::Create(
      frame->GetWebFrame()->View(), this,
      frame ? frame->GetBlinkPreferences() : blink::web_pref::WebPreferences(),
      html_data, GURL(kPluginPlaceholderDataURL));
}

const blink::WebPluginParams& PluginPlaceholderBase::GetPluginParams() const {
  return plugin_params_;
}

void PluginPlaceholderBase::ShowContextMenu(const blink::WebMouseEvent& event) {
  // Does nothing by default. Will be overridden if a specific browser wants
  // a context menu.
  return;
}

void PluginPlaceholderBase::PluginDestroyed() {
  plugin_ = nullptr;
}

v8::Local<v8::Object> PluginPlaceholderBase::GetV8ScriptableObject(
    v8::Isolate* isolate) const {
  return v8::Local<v8::Object>();
}

void PluginPlaceholderBase::HidePlugin() {
  hidden_ = true;
  if (!plugin())
    return;
  blink::WebPluginContainer* container = plugin()->Container();
  blink::WebElement element = container->GetElement();
  element.SetAttribute("style", "display: none;");
  // If we have a width and height, search for a parent (often <div>) with the
  // same dimensions. If we find such a parent, hide that as well.
  // This makes much more uncovered page content usable (including clickable)
  // as opposed to merely visible.
  // TODO(cevans) -- it's a foul heuristic but we're going to tolerate it for
  // now for these reasons:
  // 1) Makes the user experience better.
  // 2) Foulness is encapsulated within this single function.
  // 3) Confidence in no fasle positives.
  // 4) Seems to have a good / low false negative rate at this time.
  if (element.HasAttribute("width") && element.HasAttribute("height")) {
    std::string width_str("width:[\\s]*");
    width_str += element.GetAttribute("width").Utf8();
    if (base::EndsWith(width_str, "px", base::CompareCase::INSENSITIVE_ASCII)) {
      width_str = width_str.substr(0, width_str.length() - 2);
    }
    base::TrimWhitespaceASCII(width_str, base::TRIM_TRAILING, &width_str);
    width_str += "[\\s]*px";
    std::string height_str("height:[\\s]*");
    height_str += element.GetAttribute("height").Utf8();
    if (base::EndsWith(height_str, "px",
                       base::CompareCase::INSENSITIVE_ASCII)) {
      height_str = height_str.substr(0, height_str.length() - 2);
    }
    base::TrimWhitespaceASCII(height_str, base::TRIM_TRAILING, &height_str);
    height_str += "[\\s]*px";
    blink::WebNode parent = element;
    while (!parent.ParentNode().IsNull()) {
      parent = parent.ParentNode();
      if (!parent.IsElementNode())
        continue;
      element = parent.To<blink::WebElement>();
      if (element.HasAttribute("style")) {
        std::string style_str = element.GetAttribute("style").Utf8();
        if (RE2::PartialMatch(style_str, width_str) &&
            RE2::PartialMatch(style_str, height_str))
          element.SetAttribute("style", "display: none;");
      }
    }
  }
}

void PluginPlaceholderBase::HideCallback() {
  content::RenderThread::Get()->RecordAction(
      base::UserMetricsAction("Plugin_Hide_Click"));
  HidePlugin();
}

void PluginPlaceholderBase::NotifyPlaceholderReadyForTestingCallback() {
  if (!plugin())
    return;

  // Set an attribute and post an event, so browser tests can wait for the
  // placeholder to be ready to receive simulated user input.
  blink::WebElement element = plugin()->Container()->GetElement();
  element.SetAttribute("placeholderReady", "true");

  blink::WebLocalFrame* frame = element.GetDocument().GetFrame();
  base::Value value("placeholderReady");
  blink::WebSerializedScriptValue message_data =
      blink::WebSerializedScriptValue::Serialize(
          frame->GetAgentGroupScheduler()->Isolate(),
          content::V8ValueConverter::Create()->ToV8Value(
              value, frame->MainWorldScriptContext()));
  blink::WebDOMMessageEvent msg_event(message_data);

  plugin()->Container()->EnqueueMessageEvent(msg_event);
}

void PluginPlaceholderBase::OnDestruct() {}

// static
gin::WrapperInfo PluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

PluginPlaceholder::PluginPlaceholder(content::RenderFrame* render_frame,
                                     const blink::WebPluginParams& params)
    : PluginPlaceholderBase(render_frame, params) {}

PluginPlaceholder::~PluginPlaceholder() {
}

v8::Local<v8::Value> PluginPlaceholder::GetV8Handle(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

bool PluginPlaceholderBase::IsErrorPlaceholder() {
  return false;
}

gin::ObjectTemplateBuilder PluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PluginPlaceholder>::GetObjectTemplateBuilder(isolate)
      .SetMethod<void (plugins::PluginPlaceholder::*)()>(
          "hide", &PluginPlaceholder::HideCallback);
}

// static
PluginPlaceholder* PluginPlaceholder::Create(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const std::string& html_data) {
  auto* placeholder = new PluginPlaceholder(render_frame, params);
  placeholder->Init(html_data);
  return placeholder;
}

}  // namespace plugins
