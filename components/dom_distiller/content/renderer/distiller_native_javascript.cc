// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace {

// These values should agree with those in distilled_page_prefs.cc.
const float kMinFontScale = 0.4f;
const float kMaxFontScale = 3.0f;

}  // namespace

namespace dom_distiller {

DistillerNativeJavaScript::DistillerNativeJavaScript(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

DistillerNativeJavaScript::~DistillerNativeJavaScript() = default;

void DistillerNativeJavaScript::StoreIntTheme(int int_theme) {
  auto theme = static_cast<mojom::Theme>(int_theme);
  if (!mojom::IsKnownEnumValue(theme))
    return;
  distiller_js_service_->HandleStoreThemePref(theme);
}

void DistillerNativeJavaScript::StoreIntFontFamily(int int_font_family) {
  auto font_family = static_cast<mojom::FontFamily>(int_font_family);
  if (!mojom::IsKnownEnumValue(font_family))
    return;
  distiller_js_service_->HandleStoreFontFamilyPref(font_family);
}

void DistillerNativeJavaScript::StoreFloatFontScaling(float float_font_scale) {
  if (float_font_scale < kMinFontScale || float_font_scale > kMaxFontScale)
    return;
  distiller_js_service_->HandleStoreFontScalingPref(float_font_scale);
}

void DistillerNativeJavaScript::AddJavaScriptObjectToFrame(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate =
      render_frame_->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
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
      base::BindRepeating(
          &mojom::DistillerJavaScriptService::HandleDistillerOpenSettingsCall,
          base::Unretained(distiller_js_service_.get())));

  BindFunctionToObject(
      isolate, distiller_obj, "storeThemePref",
      base::BindRepeating(&DistillerNativeJavaScript::StoreIntTheme,
                          base::Unretained(this)));

  BindFunctionToObject(
      isolate, distiller_obj, "storeFontFamilyPref",
      base::BindRepeating(&DistillerNativeJavaScript::StoreIntFontFamily,
                          base::Unretained(this)));

  BindFunctionToObject(
      isolate, distiller_obj, "storeFontScalingPref",
      base::BindRepeating(&DistillerNativeJavaScript::StoreFloatFontScaling,
                          base::Unretained(this)));
}

template <typename Sig>
void DistillerNativeJavaScript::BindFunctionToObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> javascript_object,
    const std::string& name,
    const base::RepeatingCallback<Sig>& callback) {
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
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
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
