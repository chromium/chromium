// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/native_bindings_helper.h"

#include "base/check.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {

namespace {
const char kCastObjectName[] = "cast";
const char kCastPlatformObjectKey[] = "__platform__";

// Returns true if |value| is an ordinary, extensible JavaScript object on which
// defining properties will not invoke author script, interceptors, or access
// checks.
bool IsPlainExtensibleObject(v8::Isolate* isolate,
                             v8::Local<v8::Context> context,
                             v8::Local<v8::Value> value) {
  if (!value->IsObject() || value->IsProxy() ||
      value->IsModuleNamespaceObject()) {
    return false;
  }
  v8::Local<v8::Object> object = value.As<v8::Object>();
  if (object->IsApiWrapper() || object->HasNamedLookupInterceptor() ||
      object->HasIndexedLookupInterceptor()) {
    return false;
  }
  // Probe extensibility using a fresh, unforgeable Symbol so that frozen,
  // sealed, or non-extensible objects are replaced rather than reused.
  v8::Local<v8::Symbol> probe = v8::Symbol::New(isolate);
  bool can_define = false;
  if (!object->CreateDataProperty(context, probe, v8::Undefined(isolate))
           .To(&can_define) ||
      !can_define) {
    return false;
  }
  bool deleted = false;
  return object->Delete(context, probe).To(&deleted) && deleted;
}

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
  // Property accesses below must not run author script while bindings are
  // being installed: accessors, proxy traps, deferred module namespace
  // evaluation, and interceptor-backed objects could otherwise synchronously
  // detach the frame, deleting the bindings mid-install. The property is
  // therefore inspected through its descriptor, which does not invoke
  // accessors, and is only reused when it is a plain data property holding an
  // ordinary, extensible object.
  DCHECK(!parent->IsProxy() && !parent->IsModuleNamespaceObject());
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::String> v8_key = gin::StringToSymbol(isolate, key);

  v8::Local<v8::Value> descriptor_value;
  bool has_property = false;
  if (parent->GetOwnPropertyDescriptor(context, v8_key)
          .ToLocal(&descriptor_value) &&
      descriptor_value->IsObject()) {
    has_property = true;
    v8::Local<v8::Object> descriptor = descriptor_value.As<v8::Object>();
    v8::Local<v8::String> value_key = gin::StringToSymbol(isolate, "value");
    // Accessor descriptors have no own "value" property. Check for it before
    // Get() so that the lookup cannot fall through to an author-modifiable
    // prototype.
    bool is_data_property = false;
    v8::Local<v8::Value> child;
    if (descriptor->HasOwnProperty(context, value_key).To(&is_data_property) &&
        is_data_property &&
        descriptor->Get(context, value_key).ToLocal(&child) &&
        IsPlainExtensibleObject(isolate, context, child)) {
      return child.As<v8::Object>();
    }
  }

  if (has_property) {
    LOG(WARNING) << "Overwriting existing property with key " << key;
  }

  v8::Local<v8::Object> new_child_object = v8::Object::New(isolate);
  // CreateDataProperty(), unlike Set(), does not invoke accessors or proxy
  // traps.
  v8::Maybe<bool> result =
      parent->CreateDataProperty(context, v8_key, new_child_object);
  if (result.IsNothing() || !result.FromJust()) {
    LOG(ERROR) << "Failed to set new object with key " << key;
  }

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
  if (!web_frame) {
    return;
  }

  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  if (!isolate) {
    return;
  }

  // The HandleScope must be created before MainWorldScriptContext is called.
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Object> global = context->Global();
  auto weak_this = weak_factory_.GetWeakPtr();
  v8::Local<v8::Object> cast_platform =
      GetOrCreateCastPlatformObject(isolate, global);
  if (!weak_this) {
    return;
  }
  Install(cast_platform, isolate);
}

}  // namespace chromecast
