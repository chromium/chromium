// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8.h"

namespace dom_distiller {

DistillerNativeJavaScript::DistillerNativeJavaScript(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

DistillerNativeJavaScript::~DistillerNativeJavaScript() {}

void DistillerNativeJavaScript::AddJavaScriptObjectToFrame(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> distiller_obj =
      GetOrCreateDistillerObject(isolate, context);

  EnsureServiceConnected();

  // Many functions can simply call the Mojo interface directly and have no
  // wrapper function for binding. Note that calling distiller_js_service.get()
  // does not transfer ownership of the interface.
  BindFunctionToObject(
      isolate, distiller_obj, "openSettings",
      base::Bind(
          &mojom::DistillerJavaScriptService::HandleDistillerOpenSettingsCall,
          base::Unretained(distiller_js_service_.get())));
}

template <typename Sig>
void DistillerNativeJavaScript::BindFunctionToObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> javascript_object,
    const std::string& name,
    const base::Callback<Sig> callback) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  // Get the isolate associated with this object.
  javascript_object
      ->Set(context, gin::StringToSymbol(isolate, name),
            gin::CreateFunctionTemplate(isolate, callback)
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
}

void DistillerNativeJavaScript::EnsureServiceConnected() {
  if (!distiller_js_service_) {
    render_frame_->GetBrowserInterfaceBroker()->GetInterface(
        distiller_js_service_.BindNewPipeAndPassReceiver());
  }
}

v8::Local<v8::Object> GetOrCreateDistillerObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context) {
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Object> distiller_obj;
  v8::Local<v8::Value> distiller_value;
  if (!global->Get(context, gin::StringToV8(isolate, "distiller"))
           .ToLocal(&distiller_value) ||
      !distiller_value->IsObject()) {
    distiller_obj = v8::Object::New(isolate);
    global
        ->Set(context, gin::StringToSymbol(isolate, "distiller"), distiller_obj)
        .Check();
  } else {
    distiller_obj = v8::Local<v8::Object>::Cast(distiller_value);
  }
  return distiller_obj;
}

}  // namespace dom_distiller
