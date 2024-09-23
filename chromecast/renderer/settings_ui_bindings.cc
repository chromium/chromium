// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/settings_ui_bindings.h"

#include <array>
#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {
namespace shell {

namespace {

const int64_t kDelayBetweenReconnectionInMillis = 100;

const char kSettingsUiName[] = "settings_ui";
const char kSetSideSwipeHandler[] = "setSideSwipeHandler";
const char kSetPlatformInfoHandler[] = "setPlatformInfoHandler";
const char kRequestVisible[] = "requestVisible";

}  // namespace

SettingsUiBindings::SettingsUiBindings(content::RenderFrame* frame)
    : CastBinding(frame), binding_(this), weak_factory_(this) {
  ReconnectMojo();
}

SettingsUiBindings::~SettingsUiBindings() {}

void SettingsUiBindings::HandleSideSwipe(
    chromecast::mojom::SideSwipeEvent event,
    chromecast::mojom::SideSwipeOrigin origin,
    const gfx::Point& touch_location) {
  if (side_swipe_handler_.IsEmpty()) {
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler =
      v8::Local<v8::Function>::New(isolate, std::move(side_swipe_handler_));

  v8::Local<v8::Number> touch_event =
      v8::Integer::New(isolate, static_cast<int>(event));
  v8::Local<v8::Number> touch_origin =
      v8::Integer::New(isolate, static_cast<int>(origin));
  v8::Local<v8::Number> touch_x = v8::Integer::New(isolate, touch_location.x());
  v8::Local<v8::Number> touch_y = v8::Integer::New(isolate, touch_location.y());

  auto args = v8::to_array<v8::Local<v8::Value>>(
      {touch_event, touch_origin, touch_x, touch_y});

  v8::MaybeLocal<v8::Value> maybe_result =
      handler->Call(context, context->Global(), args.size(), args.data());

  side_swipe_handler_ = v8::UniquePersistent<v8::Function>(isolate, handler);

  v8::Local<v8::Value> result;
  std::ignore = maybe_result.ToLocal(&result);
}

void SettingsUiBindings::SendPlatformInfo(
    const std::string& platform_info_json) {
  if (platform_info_handler_.IsEmpty()) {
    pending_platform_info_json_ = platform_info_json;
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler =
      v8::Local<v8::Function>::New(isolate, std::move(platform_info_handler_));

  v8::Local<v8::String> platform_info =
      v8::String::NewFromUtf8(isolate, platform_info_json.data(),
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();

  auto args = v8::to_array<v8::Local<v8::Value>>({platform_info});

  v8::MaybeLocal<v8::Value> maybe_result =
      handler->Call(context, context->Global(), args.size(), args.data());

  platform_info_handler_ = v8::UniquePersistent<v8::Function>(isolate, handler);

  v8::Local<v8::Value> result;
  std::ignore = maybe_result.ToLocal(&result);
}

void SettingsUiBindings::Install(v8::Local<v8::Object> cast_platform,
                                 v8::Isolate* isolate) {
  v8::Local<v8::Object> settings_ui =
      EnsureObjectExists(isolate, cast_platform, kSettingsUiName);

  InstallBinding(isolate, settings_ui, kSetSideSwipeHandler,
                 &SettingsUiBindings::SetSideSwipeHandler,
                 base::Unretained(this));
  InstallBinding(isolate, settings_ui, kSetPlatformInfoHandler,
                 &SettingsUiBindings::SetPlatformInfoHandler,
                 base::Unretained(this));
  InstallBinding(isolate, settings_ui, kRequestVisible,
                 &SettingsUiBindings::RequestVisible, base::Unretained(this));
}

void SettingsUiBindings::SetSideSwipeHandler(
    v8::Local<v8::Function> side_swipe_handler) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  side_swipe_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, side_swipe_handler);
}

void SettingsUiBindings::SetPlatformInfoHandler(
    v8::Local<v8::Function> platform_info_handler) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  platform_info_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, platform_info_handler);
  if (!pending_platform_info_json_.empty()) {
    SendPlatformInfo(pending_platform_info_json_);
    pending_platform_info_json_.clear();
  }
}

void SettingsUiBindings::RequestVisible(bool visible) {
  if (settings_platform_ptr_) {
    settings_platform_ptr_->RequestVisible(visible);
  }
}

void SettingsUiBindings::ReconnectMojo() {
  if (binding_.is_bound())
    binding_.reset();
  if (settings_platform_ptr_.is_bound())
    settings_platform_ptr_.reset();

  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      settings_platform_ptr_.BindNewPipeAndPassReceiver());
  settings_platform_ptr_.set_disconnect_handler(base::BindOnce(
      &SettingsUiBindings::OnMojoConnectionError, weak_factory_.GetWeakPtr()));
  settings_platform_ptr_->Connect(binding_.BindNewPipeAndPassRemote());
  mojo_reconnect_timer_.Stop();
}

void SettingsUiBindings::OnMojoConnectionError() {
  LOG(WARNING) << "Disconnected from settings UI MOJO. Will reconnect every "
               << kDelayBetweenReconnectionInMillis << " milliseconds.";
  mojo_reconnect_timer_.Start(
      FROM_HERE, base::Milliseconds(kDelayBetweenReconnectionInMillis), this,
      &SettingsUiBindings::ReconnectMojo);
}

}  // namespace shell
}  // namespace chromecast
