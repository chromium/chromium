// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/native_bindings_helper.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {

namespace {
const char kCastObjectName[] = "cast";
const char kCastPlatformObjectKey[] = "__platform__";
}  // namespace

v8::Local<v8::Object> GetOrCreateCastPlatformObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> global) {
  v8::Local<v8::Object> cast =
      EnsureObjectExists(isolate, global, kCastObjectName);
  return EnsureObjectExists(isolate, cast, kCastPlatformObjectKey);
}

v8::Local<v8::Object> EnsureObjectExists(v8::Isolate* isolate,
                                         v8::Local<v8::Object> parent,
                                         const std::string& key) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MaybeLocal<v8::Value> child =
      parent->Get(context, gin::StringToV8(isolate, key));
  v8::Local<v8::Value> child_local;
  v8::Local<v8::Object> child_object;
  if (child.ToLocal(&child_local) && child_local->IsObject() &&
      child_local->ToObject(context).ToLocal(&child_object))
    return child_object;

  if (!child_local.IsEmpty() && !child_local->IsUndefined())
    LOG(WARNING) << "Overwriting non-empty non-object with key " << key;

  v8::Local<v8::Object> new_child_object = v8::Object::New(isolate);
  v8::Maybe<bool> result =
      parent->Set(context, gin::StringToSymbol(isolate, key), new_child_object);
  if (result.IsNothing() || !result.FromJust())
    LOG(ERROR) << "Failed to set new object with key " << key;

  return new_child_object;
}

CastBinding::CastBinding(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

CastBinding::~CastBinding() {}

void CastBinding::DidClearWindowObject() {
  TryInstall();
}

void CastBinding::OnDestruct() {
  delete this;
}

void CastBinding::TryInstall() {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (!web_frame)
    return;

  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  if (!isolate)
    return;

  // The HandleScope must be created before MainWorldScriptContext is called.
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Object> cast_platform =
      GetOrCreateCastPlatformObject(isolate, global);
  Install(cast_platform, isolate);
}

}  // namespace chromecast
