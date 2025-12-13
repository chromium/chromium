// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webui_browser/webui_browser_renderer_extension.h"

#include "base/check.h"
#include "chrome/common/url_constants.h"
#include "components/guest_contents/renderer/swap_render_frame.h"
#include "content/public/common/isolated_world_ids.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/public/web/web_document.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace {

// Expose the API to only chrome://webui-browser/*.
bool ShouldExposeWebUIBrowserApi(content::RenderFrame* render_frame) {
  CHECK(render_frame);
  const url::Origin webui_browser_origin =
      url::Origin::Create(GURL(chrome::kChromeUIWebuiBrowserURL));
  return url::Origin::Create(render_frame->GetWebFrame()->GetDocument().Url())
      .IsSameOriginWith(webui_browser_origin);
}

// Implementation of chrome.browser.allowCustomElementRegistration(callback)
void AllowCustomElementNameRegistration(v8::Local<v8::Function> callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  blink::WebCustomElement::EmbedderNamesAllowedScope embedder_names_scope;
  callback->Call(context, context->Global(), 0, nullptr).ToLocalChecked();
}

content::RenderFrame* GetRenderFrame(v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context;
  if (!v8::Local<v8::Object>::Cast(value)->GetCreationContext().ToLocal(
          &context)) {
    if (context.IsEmpty()) {
      return nullptr;
    }
  }
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForContext(context);
  if (!frame) {
    return nullptr;
  }
  return content::RenderFrame::FromWebFrame(frame);
}

// Implementation of
// chrome.browser.attachIframeGuest(guestInstanceId, contentWindow)
void AttachIframeGuest(int guest_contents_id,
                       v8::Local<v8::Object> content_window) {
  content::RenderFrame* iframe_render_frame = GetRenderFrame(content_window);
  CHECK(iframe_render_frame);

  blink::WebFrame* parent_frame = iframe_render_frame->GetWebFrame()->Parent();
  CHECK(parent_frame);
  CHECK(parent_frame->IsWebLocalFrame());

  guest_contents::renderer::SwapRenderFrame(iframe_render_frame,
                                            guest_contents_id);
}

}  // namespace

// static
void WebUIBrowserRendererExtension::Create(content::RenderFrame* frame) {
  new WebUIBrowserRendererExtension(frame);
}

WebUIBrowserRendererExtension::WebUIBrowserRendererExtension(
    content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

WebUIBrowserRendererExtension::~WebUIBrowserRendererExtension() = default;

void WebUIBrowserRendererExtension::OnDestruct() {
  delete this;
}

void WebUIBrowserRendererExtension::DidCreateScriptContext(
    v8::Local<v8::Context> v8_context,
    int32_t world_id) {
  if (!render_frame() || world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeWebUIBrowserApi(render_frame())) {
    InjectScript();
  }
}

void WebUIBrowserRendererExtension::InjectScript() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> chrome_value;
  if (!global->Get(context, gin::StringToV8(isolate, "chrome"))
           .ToLocal(&chrome_value)) {
    chrome_value = v8::Object::New(isolate);
    global->Set(context, gin::StringToV8(isolate, "chrome"), chrome_value)
        .Check();
  }
  CHECK(chrome_value->IsObject());
  v8::Local<v8::Object> chrome_obj = v8::Local<v8::Object>::Cast(chrome_value);

  v8::Local<v8::ObjectTemplate> browser_template =
      gin::ObjectTemplateBuilder(isolate)
          .SetMethod("allowCustomElementRegistration",
                     &AllowCustomElementNameRegistration)
          .SetMethod("attachIframeGuest", &AttachIframeGuest)
          .Build();
  chrome_obj
      ->Set(context, gin::StringToV8(isolate, "browser"),
            browser_template->NewInstance(context).ToLocalChecked())
      .FromJust();
}
