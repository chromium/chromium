// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_window_manager_bindings.h"

#include <array>
#include <tuple>

#include "base/check.h"
#include "build/build_config.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/common/feature_constants.h"
#include "chromecast/common/mojom/gesture.mojom.h"
#include "chromecast/renderer/feature_manager.h"
#include "content/public/renderer/render_frame.h"
#include "gin/data_object_builder.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace chromecast {
namespace shell {

namespace {

const char kBindingsObjectName[] = "windowManager";
const char kOnBackGestureName[] = "onBackGesture";
const char kOnBackGestureProgressName[] = "onBackGestureProgress";
const char kOnBackGestureCancelName[] = "onBackGestureCancel";
const char kOnTopDragGestureDoneName[] = "onTopDragGestureDone";
const char kOnTopDragGestureProgressName[] = "onTopDragGestureProgress";
const char kOnRightDragGestureDoneName[] = "onRightDragGestureDone";
const char kOnRightDragGestureProgressName[] = "onRightDragGestureProgress";
const char kOnTapGestureName[] = "onTapGesture";
const char kOnTapDownGestureName[] = "onTapDownGesture";
const char kCanGoBackName[] = "canGoBack";
const char kCanTopDragName[] = "canTopDrag";
const char kCanRightDragName[] = "canRightDrag";
const char kMinimize[] = "minimize";
const char kMaximize[] = "maximize";

#if !BUILDFLAG(IS_ANDROID)
const char kDisplayControlsName[] = "displayControls";
#endif

void OnGestureSourceDisconnectionError() {
  LOG(ERROR) << "Connection error talking to system gesture source.";
}

void OnWindowDisconnect() {
  LOG(ERROR) << "Connection error talking to platform activity.";
}

}  // namespace

CastWindowManagerBindings::CastWindowManagerBindings(
    content::RenderFrame* render_frame,
    const FeatureManager* feature_manager)
    : CastBinding(render_frame),
      feature_manager_(feature_manager),
      handler_receiver_(this) {
  DCHECK(feature_manager_);
}

CastWindowManagerBindings::~CastWindowManagerBindings() {}

void CastWindowManagerBindings::Install(v8::Local<v8::Object> cast_platform,
                                        v8::Isolate* isolate) {
  if (feature_manager_->FeatureEnabled(feature::kEnableSystemGestures)) {
    v8::Local<v8::Object> windowManagerObject =
        EnsureObjectExists(isolate, cast_platform, kBindingsObjectName);

    // On back bindings.
    InstallBinding(isolate, windowManagerObject, kCanGoBackName,
                   &CastWindowManagerBindings::SetCanGoBack,
                   base::Unretained(this));
    InstallBinding(isolate, windowManagerObject, kOnBackGestureName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this), &on_back_gesture_callback_);
    InstallBinding(isolate, windowManagerObject, kOnBackGestureProgressName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this), &on_back_gesture_progress_callback_);
    InstallBinding(isolate, windowManagerObject, kOnBackGestureCancelName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this), &on_back_gesture_cancel_callback_);

    // Top drag bindings.
    InstallBinding(isolate, windowManagerObject, kCanTopDragName,
                   &CastWindowManagerBindings::SetCanTopDrag,
                   base::Unretained(this));
    InstallBinding(isolate, windowManagerObject, kOnTopDragGestureDoneName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this), &on_top_drag_gesture_done_callback_);
    InstallBinding(isolate, windowManagerObject, kOnTopDragGestureProgressName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this),
                   &on_top_drag_gesture_progress_callback_);

    // Right drag bindings.
    InstallBinding(isolate, windowManagerObject, kCanRightDragName,
                   &CastWindowManagerBindings::SetCanRightDrag,
                   base::Unretained(this));
    InstallBinding(isolate, windowManagerObject, kOnRightDragGestureDoneName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this),
                   &on_right_drag_gesture_done_callback_);
    InstallBinding(
        isolate, windowManagerObject, kOnRightDragGestureProgressName,
        &CastWindowManagerBindings::SetV8Callback, base::Unretained(this),
        &on_right_drag_gesture_progress_callback_);

    // 'Tap' bindings.
    InstallBinding(isolate, windowManagerObject, kOnTapGestureName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this), &on_tap_gesture_callback_);
    InstallBinding(isolate, windowManagerObject, kOnTapDownGestureName,
                   &CastWindowManagerBindings::SetV8Callback,
                   base::Unretained(this), &on_tap_down_gesture_callback_);
  }
  if (feature_manager_->FeatureEnabled(feature::kEnableWindowControls)) {
    v8::Local<v8::Object> windowManagerObject =
        EnsureObjectExists(isolate, cast_platform, kBindingsObjectName);

    InstallBinding(isolate, windowManagerObject, kMaximize,
                   &CastWindowManagerBindings::Show, base::Unretained(this));
    InstallBinding(isolate, windowManagerObject, kMinimize,
                   &CastWindowManagerBindings::Hide, base::Unretained(this));
  }
}

v8::Local<v8::Value> CastWindowManagerBindings::SetV8Callback(
    v8::UniquePersistent<v8::Function>* callback_function,
    v8::Local<v8::Function> callback) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();

  *callback_function = v8::UniquePersistent<v8::Function>(isolate, callback);

  return v8::Undefined(isolate);
}

void CastWindowManagerBindings::Show() {
  BindWindow();
  window_->Show();
}

void CastWindowManagerBindings::Hide() {
  BindWindow();
  window_->Hide();
}

void CastWindowManagerBindings::SetCanGoBack(bool can_go_back) {
  BindGestureSource();
  gesture_source_->SetCanGoBack(can_go_back);
}

void CastWindowManagerBindings::SetCanTopDrag(bool can_top_drag) {
  BindGestureSource();
  gesture_source_->SetCanTopDrag(can_top_drag);
}

void CastWindowManagerBindings::SetCanRightDrag(bool can_right_drag) {
  BindGestureSource();
  gesture_source_->SetCanRightDrag(can_right_drag);
}

void CastWindowManagerBindings::OnTouchInputSupportSet(
    PersistedResolver resolver,
    PersistedContext original_context,
    bool resolve_promise,
    bool display_controls) {
  DVLOG(2) << __FUNCTION__;
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  if (resolve_promise) {
    resolver.Get(isolate)
        ->Resolve(context,

#if !BUILDFLAG(IS_ANDROID)
                  gin::DataObjectBuilder(isolate)
                      .Set(kDisplayControlsName, display_controls)
                      .Build()
#else
                  v8::Undefined(isolate)
#endif
                      )
        .ToChecked();
  } else {
    resolver.Get(isolate)->Reject(context, v8::Undefined(isolate)).ToChecked();
  }
}

void CastWindowManagerBindings::BindGestureSource() {
  if (gesture_source_.is_bound() && gesture_source_.is_connected())
    return;
  gesture_source_.reset();
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      gesture_source_.BindNewPipeAndPassReceiver());
  gesture_source_.set_disconnect_handler(
      base::BindRepeating(&OnGestureSourceDisconnectionError));
  handler_receiver_.reset();
  gesture_source_->Subscribe(handler_receiver_.BindNewPipeAndPassRemote());
}

void CastWindowManagerBindings::BindWindow() {
  if (window_.is_bound() && window_.is_connected())
    return;
  window_.reset();
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      window_.BindNewPipeAndPassReceiver());
  window_.set_disconnect_handler(base::BindRepeating(&OnWindowDisconnect));
}

void CastWindowManagerBindings::InvokeV8Callback(
    v8::UniquePersistent<v8::Function>* callback_function) {
  if (callback_function->IsEmpty()) {
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler =
      v8::Local<v8::Function>::New(isolate, std::move(*callback_function));

  v8::MaybeLocal<v8::Value> maybe_result =
      handler->Call(context, context->Global(), 0, nullptr);

  *callback_function = v8::UniquePersistent<v8::Function>(isolate, handler);

  v8::Local<v8::Value> result;
  std::ignore = maybe_result.ToLocal(&result);
}

void CastWindowManagerBindings::InvokeV8Callback(
    v8::UniquePersistent<v8::Function>* callback_function,
    const gfx::Point& touch_location) {
  if (callback_function->IsEmpty()) {
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler =
      v8::Local<v8::Function>::New(isolate, std::move(*callback_function));

  v8::Local<v8::Number> touch_x = v8::Integer::New(isolate, touch_location.x());
  v8::Local<v8::Number> touch_y = v8::Integer::New(isolate, touch_location.y());

  auto args = v8::to_array<v8::Local<v8::Value>>({touch_x, touch_y});

  v8::MaybeLocal<v8::Value> maybe_result =
      handler->Call(context, context->Global(), args.size(), args.data());

  *callback_function = v8::UniquePersistent<v8::Function>(isolate, handler);

  v8::Local<v8::Value> result;
  std::ignore = maybe_result.ToLocal(&result);
}

void CastWindowManagerBindings::OnBackGesture(
    ::chromecast::mojom::GestureHandler::OnBackGestureCallback callback) {
  // Note: Can't use InvokeV8Callback here because of the OnBackGestureCallback
  // argument. So we have this boilerplate here until we can get rid of the
  // unused callback argument.
  if (on_back_gesture_callback_.IsEmpty()) {
    std::move(callback).Run(false);
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler = v8::Local<v8::Function>::New(
      isolate, std::move(on_back_gesture_callback_));
  auto result = handler->Call(context, context->Global(), 0, nullptr);

  on_back_gesture_callback_ =
      v8::UniquePersistent<v8::Function>(isolate, handler);
  if (result.IsEmpty()) {
    LOG(ERROR) << "No value from callback execution; ";
    std::move(callback).Run(false);
    return;
  }

  auto callback_return_value = result.ToLocalChecked();
  bool return_boolean = callback_return_value->BooleanValue(isolate);
  std::move(callback).Run(return_boolean);
}

void CastWindowManagerBindings::OnBackGestureProgress(
    const gfx::Point& touch_location) {
  InvokeV8Callback(&on_back_gesture_progress_callback_, touch_location);
}

void CastWindowManagerBindings::OnBackGestureCancel() {
  InvokeV8Callback(&on_back_gesture_cancel_callback_);
}

void CastWindowManagerBindings::OnTopDragGestureDone() {
  InvokeV8Callback(&on_top_drag_gesture_done_callback_);
}

void CastWindowManagerBindings::OnTopDragGestureProgress(
    const gfx::Point& touch_location) {
  InvokeV8Callback(&on_top_drag_gesture_progress_callback_, touch_location);
}

void CastWindowManagerBindings::OnRightDragGestureDone() {
  InvokeV8Callback(&on_right_drag_gesture_done_callback_);
}

void CastWindowManagerBindings::OnRightDragGestureProgress(
    const gfx::Point& touch_location) {
  InvokeV8Callback(&on_right_drag_gesture_progress_callback_, touch_location);
}

void CastWindowManagerBindings::OnTapGesture() {
  InvokeV8Callback(&on_tap_gesture_callback_);
}

void CastWindowManagerBindings::OnTapDownGesture() {
  InvokeV8Callback(&on_tap_down_gesture_callback_);
}

}  // namespace shell
}  // namespace chromecast
