// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_demo_bindings.h"

#include <tuple>

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {
namespace shell {

namespace {
const int64_t kDelayBetweenReconnectionInMillis = 100;

const char kDemoObjectName[] = "demo";

const char kRecordEventName[] = "recordEvent";
const char kSetRetailerName[] = "setRetailer";
const char kSetStoreIdName[] = "setStoreId";
const char kGetRetailerName[] = "getRetailer";
const char kGetStoreIdName[] = "getStoreId";
const char kSetDefaultVolumeName[] = "setDefaultVolume";
const char kGetDefaultVolumeName[] = "getDefaultVolume";
const char kApplyDefaultVolumeName[] = "applyDefaultVolume";
const char kSetWifiConnectionName[] = "setWifiConnection";
const char kGetAvailableWifiNetworksName[] = "getAvailableWifiNetworks";
const char kGetWifiConnectionStateName[] = "getWifiConnectionState";
const char kRegisterVolumeChangeHandlerName[] = "registerVolumeChangeHandler";
const char kPersistLocalStorageName[] = "persistLocalStorage";

const char kSetVolumeName[] = "setVolume";
}  // namespace

CastDemoBindings::CastDemoBindings(content::RenderFrame* render_frame)
    : CastBinding(render_frame), binding_(this), weak_factory_(this) {}

CastDemoBindings::~CastDemoBindings() {}

void CastDemoBindings::Install(v8::Local<v8::Object> cast_platform,
                               v8::Isolate* isolate) {
  v8::Local<v8::Object> demo_object =
      EnsureObjectExists(isolate, cast_platform, kDemoObjectName);

  InstallBinding(isolate, demo_object, kRecordEventName,
                 &CastDemoBindings::RecordEvent, base::Unretained(this));
  InstallBinding(isolate, demo_object, kSetRetailerName,
                 &CastDemoBindings::SetRetailerName, base::Unretained(this));
  InstallBinding(isolate, demo_object, kSetStoreIdName,
                 &CastDemoBindings::SetStoreId, base::Unretained(this));
  InstallBinding(isolate, demo_object, kGetRetailerName,
                 &CastDemoBindings::GetRetailerName, base::Unretained(this));
  InstallBinding(isolate, demo_object, kGetStoreIdName,
                 &CastDemoBindings::GetStoreId, base::Unretained(this));
  InstallBinding(isolate, demo_object, kSetDefaultVolumeName,
                 &CastDemoBindings::SetDefaultVolumeLevel,
                 base::Unretained(this));
  InstallBinding(isolate, demo_object, kGetDefaultVolumeName,
                 &CastDemoBindings::GetDefaultVolumeLevel,
                 base::Unretained(this));
  InstallBinding(isolate, demo_object, kApplyDefaultVolumeName,
                 &CastDemoBindings::ApplyDefaultVolume, base::Unretained(this));
  InstallBinding(isolate, demo_object, kSetWifiConnectionName,
                 &CastDemoBindings::SetWifiCredentials, base::Unretained(this));
  InstallBinding(isolate, demo_object, kGetAvailableWifiNetworksName,
                 &CastDemoBindings::GetAvailableWifiNetworks,
                 base::Unretained(this));
  InstallBinding(isolate, demo_object, kGetWifiConnectionStateName,
                 &CastDemoBindings::GetConnectionStatus,
                 base::Unretained(this));
  InstallBinding(isolate, demo_object, kRegisterVolumeChangeHandlerName,
                 &CastDemoBindings::SetVolumeChangeHandler,
                 base::Unretained(this));
  InstallBinding(isolate, demo_object, kPersistLocalStorageName,
                 &CastDemoBindings::PersistLocalStorage,
                 base::Unretained(this));

  InstallBinding(isolate, demo_object, kSetVolumeName,
                 &CastDemoBindings::SetVolume, base::Unretained(this));
}

void CastDemoBindings::RecordEvent(const std::string& event_name,
                                   v8::Local<v8::Value> v8_data) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* v8_isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope v8_handle_scope(v8_isolate);
  v8::Local<v8::Context> v8_context = web_frame->MainWorldScriptContext();
  v8::Context::Scope v8_context_scope(v8_context);

  std::unique_ptr<content::V8ValueConverter> v8_converter =
      content::V8ValueConverter::Create();
  v8_converter->SetDateAllowed(true);
  v8_converter->SetRegExpAllowed(true);
  std::unique_ptr<base::Value> data_ptr =
      v8_converter->FromV8Value(v8_data, v8_context);

  base::Value data;
  if (data_ptr) {
    data = base::Value::FromUniquePtrValue(std::move(data_ptr));
  }
  GetCastDemo()->RecordEvent(event_name, std::move(data));
}

void CastDemoBindings::SetRetailerName(const std::string& retailer_name) {
  GetCastDemo()->SetRetailerName(retailer_name);
}

void CastDemoBindings::SetStoreId(const std::string& store_id) {
  GetCastDemo()->SetStoreId(store_id);
}

v8::Local<v8::Value> CastDemoBindings::GetRetailerName() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::Global<v8::Promise::Resolver> unique_resolver =
      v8::Global<v8::Promise::Resolver>(isolate, resolver);
  v8::Global<v8::Context> persisted_context =
      v8::Global<v8::Context>(isolate, context);

  GetCastDemo()->GetRetailerName(base::BindOnce(
      &CastDemoBindings::OnGetRetailerName, base::Unretained(this),
      std::move(unique_resolver), std::move(persisted_context)));
  return resolver->GetPromise();
}

void CastDemoBindings::OnGetRetailerName(
    v8::Global<v8::Promise::Resolver> resolver,
    v8::Global<v8::Context> original_context,
    const std::string& retailer_name) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  resolver.Get(isolate)
      ->Resolve(context, gin::ConvertToV8(isolate, retailer_name))
      .ToChecked();
}

v8::Local<v8::Value> CastDemoBindings::GetStoreId() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::Global<v8::Promise::Resolver> unique_resolver =
      v8::Global<v8::Promise::Resolver>(isolate, resolver);
  v8::Global<v8::Context> persisted_context =
      v8::Global<v8::Context>(isolate, context);

  GetCastDemo()->GetStoreId(
      base::BindOnce(&CastDemoBindings::OnGetStoreId, base::Unretained(this),
                     std::move(unique_resolver), std::move(persisted_context)));
  return resolver->GetPromise();
}

void CastDemoBindings::OnGetStoreId(v8::Global<v8::Promise::Resolver> resolver,
                                    v8::Global<v8::Context> original_context,
                                    const std::string& store_id) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  resolver.Get(isolate)
      ->Resolve(context, gin::ConvertToV8(isolate, store_id))
      .ToChecked();
}

void CastDemoBindings::SetVolume(float level) {
  // This method is deprecated. Provide a workable implementation to support
  // development using old content.
  SetDefaultVolumeLevel(level);
  ApplyDefaultVolume();
}

void CastDemoBindings::SetDefaultVolumeLevel(float level) {
  GetCastDemo()->SetDefaultVolumeLevel(level);
}

v8::Local<v8::Value> CastDemoBindings::GetDefaultVolumeLevel() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::Global<v8::Promise::Resolver> unique_resolver =
      v8::Global<v8::Promise::Resolver>(isolate, resolver);
  v8::Global<v8::Context> persisted_context =
      v8::Global<v8::Context>(isolate, context);

  GetCastDemo()->GetDefaultVolumeLevel(base::BindOnce(
      &CastDemoBindings::OnGetDefaultVolumeLevel, base::Unretained(this),
      std::move(unique_resolver), std::move(persisted_context)));
  return resolver->GetPromise();
}

void CastDemoBindings::OnGetDefaultVolumeLevel(
    v8::Global<v8::Promise::Resolver> resolver,
    v8::Global<v8::Context> original_context,
    float level) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  resolver.Get(isolate)
      ->Resolve(context, gin::ConvertToV8(isolate, level))
      .ToChecked();
}

void CastDemoBindings::ApplyDefaultVolume() {
  GetCastDemo()->ApplyDefaultVolume();
}

void CastDemoBindings::SetWifiCredentials(const std::string& ssid,
                                          const std::string& psk) {
  GetCastDemo()->SetWifiCredentials(ssid, psk);
}

v8::Local<v8::Value> CastDemoBindings::GetAvailableWifiNetworks() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::Global<v8::Promise::Resolver> unique_resolver =
      v8::Global<v8::Promise::Resolver>(isolate, resolver);
  v8::Global<v8::Context> persisted_context =
      v8::Global<v8::Context>(isolate, context);

  GetCastDemo()->GetAvailableWifiNetworks(base::BindOnce(
      &CastDemoBindings::OnGetAvailableWifiNetworks, base::Unretained(this),
      std::move(unique_resolver), std::move(persisted_context)));
  return resolver->GetPromise();
}

void CastDemoBindings::OnGetAvailableWifiNetworks(
    v8::Global<v8::Promise::Resolver> resolver,
    v8::Global<v8::Context> original_context,
    base::Value network_list) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  std::unique_ptr<content::V8ValueConverter> v8_converter =
      content::V8ValueConverter::Create();
  v8::Local<v8::Value> v8_value =
      v8_converter->ToV8Value(network_list, context);

  resolver.Get(isolate)
      ->Resolve(context, gin::ConvertToV8(isolate, v8_value))
      .ToChecked();
}

v8::Local<v8::Value> CastDemoBindings::GetConnectionStatus() {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  auto context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::Global<v8::Promise::Resolver> unique_resolver =
      v8::Global<v8::Promise::Resolver>(isolate, resolver);
  v8::Global<v8::Context> persisted_context =
      v8::Global<v8::Context>(isolate, context);

  GetCastDemo()->GetConnectionStatus(base::BindOnce(
      &CastDemoBindings::OnGetConnectionStatus, base::Unretained(this),
      std::move(unique_resolver), std::move(persisted_context)));
  return resolver->GetPromise();
}

void CastDemoBindings::OnGetConnectionStatus(
    v8::Global<v8::Promise::Resolver> resolver,
    v8::Global<v8::Context> original_context,
    base::Value status) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = original_context.Get(isolate);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  std::unique_ptr<content::V8ValueConverter> v8_converter =
      content::V8ValueConverter::Create();
  v8::Local<v8::Value> v8_value = v8_converter->ToV8Value(status, context);

  resolver.Get(isolate)
      ->Resolve(context, gin::ConvertToV8(isolate, v8_value))
      .ToChecked();
}

void CastDemoBindings::SetVolumeChangeHandler(
    v8::Local<v8::Function> volume_change_handler) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  volume_change_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, volume_change_handler);
}

void CastDemoBindings::VolumeChanged(float level) {
  if (volume_change_handler_.IsEmpty()) {
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> handler =
      v8::Local<v8::Function>::New(isolate, std::move(volume_change_handler_));

  auto args =
      v8::to_array<v8::Local<v8::Value>>({gin::ConvertToV8(isolate, level)});

  v8::MaybeLocal<v8::Value> maybe_result =
      handler->Call(context, context->Global(), args.size(), args.data());

  volume_change_handler_ = v8::UniquePersistent<v8::Function>(isolate, handler);

  v8::Local<v8::Value> result;
  std::ignore = maybe_result.ToLocal(&result);
}

void CastDemoBindings::PersistLocalStorage() {
  GetCastDemo()->PersistLocalStorage();
}

void CastDemoBindings::ReconnectMojo() {
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      cast_demo_.BindNewPipeAndPassReceiver());
  DCHECK(cast_demo_.is_bound());
  cast_demo_.set_disconnect_handler(base::BindOnce(
      &CastDemoBindings::OnMojoConnectionError, base::Unretained(this)));

  if (binding_.is_bound()) {
    binding_.reset();
  }

  mojo::PendingRemote<mojom::CastDemoVolumeChangeObserver> pending_remote;
  binding_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
  cast_demo_->AddVolumeChangeObserver(std::move(pending_remote));
}

void CastDemoBindings::OnMojoConnectionError() {
  LOG(WARNING) << "Disconnected from Demo Mojo. Will retry every "
               << kDelayBetweenReconnectionInMillis << " milliseconds.";
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastDemoBindings::ReconnectMojo,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(kDelayBetweenReconnectionInMillis));
}

const mojo::Remote<mojom::CastDemo>& CastDemoBindings::GetCastDemo() {
  if (!cast_demo_.is_bound()) {
    ReconnectMojo();
  }
  return cast_demo_;
}

}  // namespace shell
}  // namespace chromecast
