// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_accessibility_bindings.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chromecast/common/feature_constants.h"
#include "chromecast/renderer/feature_manager.h"
#include "content/public/renderer/render_frame.h"
#include "gin/data_object_builder.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {
namespace shell {

namespace {

constexpr char kBindingsObjectName[] = "accessibility";
constexpr char kSetColorInversionName[] = "setColorInversion";
constexpr char kSetScreenReaderName[] = "setScreenReader";
constexpr char kSetMagnificationGestureName[] = "setMagnificationGesture";
constexpr char kGetAccessibilitySettings[] = "getAccessibilitySettings";
constexpr char kIsColorInversionEnabled[] = "isColorInversionEnabled";
constexpr char kIsScreenReaderEnabled[] = "isScreenReaderEnabled";
constexpr char kIsMagnificationGestureEnabled[] =
    "isMagnificationGestureEnabled";
constexpr char kScreenReaderSettingChangedHandler[] =
    "setScreenReaderSettingChangedHandler";
constexpr char kColorInversionSettingChangedHandler[] =
    "setColorInversionSettingChangedHandler";
constexpr char kMagnificationGestureSettingChangedHandler[] =
    "setMagnificationGestureSettingChangedHandler";

void OnAccessibilityServiceConnectionError() {
  LOG(ERROR) << "Connection error talking to accessibility manager.";
}

}  // namespace

CastAccessibilityBindings::CastAccessibilityBindings(
    content::RenderFrame* render_frame,
    const FeatureManager* feature_manager)
    : CastBinding(render_frame), feature_manager_(feature_manager) {
  registry_.AddInterface(base::BindRepeating(
      &CastAccessibilityBindings::OnCastAccessibilityClientRequest,
      base::Unretained(this)));
}

CastAccessibilityBindings::~CastAccessibilityBindings() {}

void CastAccessibilityBindings::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void CastAccessibilityBindings::Install(v8::Local<v8::Object> cast_platform,
                                        v8::Isolate* isolate) {
  v8::Local<v8::Object> AccessibilityObject =
      EnsureObjectExists(isolate, cast_platform, kBindingsObjectName);

  if (feature_manager_->FeatureEnabled(feature::kEnableAccessibilityControls)) {
    InstallBinding(isolate, AccessibilityObject, kSetColorInversionName,
                   &CastAccessibilityBindings::SetColorInversion,
                   base::Unretained(this));
    InstallBinding(isolate, AccessibilityObject, kSetScreenReaderName,
                   &CastAccessibilityBindings::SetScreenReader,
                   base::Unretained(this));
    InstallBinding(isolate, AccessibilityObject, kSetMagnificationGestureName,
                   &CastAccessibilityBindings::SetMagnificationGesture,
                   base::Unretained(this));
  }
  // Getters and callback are always available.
  InstallBinding(isolate, AccessibilityObject, kGetAccessibilitySettings,
                 &CastAccessibilityBindings::GetAccessibilitySettings,
                 base::Unretained(this));
  InstallBinding(
      isolate, AccessibilityObject, kScreenReaderSettingChangedHandler,
      &CastAccessibilityBindings::SetScreenReaderSettingChangedHandler,
      base::Unretained(this));
  InstallBinding(
      isolate, AccessibilityObject, kColorInversionSettingChangedHandler,
      &CastAccessibilityBindings::SetColorInversionSettingChangedHandler,
      base::Unretained(this));
  InstallBinding(
      isolate, AccessibilityObject, kMagnificationGestureSettingChangedHandler,
      &CastAccessibilityBindings::SetMagnificationGestureSettingChangedHandler,
      base::Unretained(this));
}

void CastAccessibilityBindings::SetColorInversion(bool enable) {
  if (!BindAccessibility())
    return;
  accessibility_service_->SetColorInversion(enable);
}

void CastAccessibilityBindings::SetScreenReader(bool enable) {
  if (!BindAccessibility())
    return;
  accessibility_service_->SetScreenReader(enable);
}

void CastAccessibilityBindings::SetMagnificationGesture(bool enable) {
  if (!BindAccessibility())
    return;
  accessibility_service_->SetMagnificationGestureEnabled(enable);
}

v8::Local<v8::Value> CastAccessibilityBindings::GetAccessibilitySettings() {
  DVLOG(2) << __FUNCTION__;
  v8::Isolate* isolate = blink::MainThreadIsolate();

  if (!BindAccessibility())
    return gin::ConvertToV8(isolate, false);

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  PersistedResolver unique_resolver =
      v8::Global<v8::Promise::Resolver>(isolate, resolver);
  PersistedContext context(isolate, isolate->GetCurrentContext());

  accessibility_service_->GetAccessibilitySettings(base::BindOnce(
      &CastAccessibilityBindings::OnGetAccessibilitySettings,
      base::Unretained(this), std::move(unique_resolver), std::move(context)));
  return resolver->GetPromise();
}

void CastAccessibilityBindings::SetScreenReaderSettingChangedHandler(
    v8::Local<v8::Function> handler) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  screen_reader_setting_changed_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, handler);
}

void CastAccessibilityBindings::SetColorInversionSettingChangedHandler(
    v8::Local<v8::Function> handler) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  color_inversion_setting_changed_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, handler);
}

void CastAccessibilityBindings::SetMagnificationGestureSettingChangedHandler(
    v8::Local<v8::Function> handler) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  magnification_gesture_setting_changed_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, handler);
}

bool CastAccessibilityBindings::BindAccessibility() {
  if (!accessibility_service_) {
    render_frame()->GetBrowserInterfaceBroker()->GetInterface(
        accessibility_service_.BindNewPipeAndPassReceiver());
    if (!accessibility_service_) {
      LOG(ERROR)
          << "Couldn't establish connection to cast window manager service";
      return false;
    }
    accessibility_service_.set_disconnect_handler(
        base::BindRepeating(&OnAccessibilityServiceConnectionError));
  }
  return true;
}

void CastAccessibilityBindings::OnGetAccessibilitySettings(
    PersistedResolver resolver,
    PersistedContext original_context,
    mojom::AccessibilitySettingsPtr settings) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> obj =
      gin::DataObjectBuilder(isolate)
          .Set(kIsColorInversionEnabled, settings->color_inversion_enabled)
          .Set(kIsScreenReaderEnabled, settings->screen_reader_enabled)
          .Set(kIsMagnificationGestureEnabled,
               settings->magnification_gesture_enabled)
          .Build();

  resolver.Get(isolate)->Resolve(context, obj).ToChecked();
}

void CastAccessibilityBindings::AccessibilitySettingChanged(
    v8::UniquePersistent<v8::Function>* handler_function,
    bool new_value) {
  if (handler_function->IsEmpty()) {
    return;
  }

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler =
      v8::Local<v8::Function>::New(isolate, std::move(*handler_function));

  v8::Local<v8::Boolean> arg = v8::Boolean::New(isolate, new_value);

  std::vector<v8::Local<v8::Value>> args{arg};

  v8::MaybeLocal<v8::Value> maybe_result =
      handler->Call(context, context->Global(), args.size(), args.data());

  *handler_function = v8::UniquePersistent<v8::Function>(isolate, handler);

  v8::Local<v8::Value> result;
  std::ignore = maybe_result.ToLocal(&result);
}

void CastAccessibilityBindings::ScreenReaderSettingChanged(bool new_value) {
  AccessibilitySettingChanged(&screen_reader_setting_changed_handler_,
                              new_value);
}

void CastAccessibilityBindings::ColorInversionSettingChanged(bool new_value) {
  AccessibilitySettingChanged(&color_inversion_setting_changed_handler_,
                              new_value);
}

void CastAccessibilityBindings::MagnificationGestureSettingChanged(
    bool new_value) {
  AccessibilitySettingChanged(&magnification_gesture_setting_changed_handler_,
                              new_value);
}

void CastAccessibilityBindings::OnCastAccessibilityClientRequest(
    mojo::PendingReceiver<shell::mojom::CastAccessibilityClient> request) {
  bindings_.Add(this, std::move(request));
}

}  // namespace shell
}  // namespace chromecast
