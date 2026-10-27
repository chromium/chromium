// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_NATIVE_BINDINGS_HELPER_H_
#define CHROMECAST_RENDERER_NATIVE_BINDINGS_HELPER_H_

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/function_template.h"
#include "v8/include/v8.h"
namespace content {
class RenderFrame;
}

namespace chromecast {

// Returns the reference to "cast.__platform__", creating it if necessary.
v8::Local<v8::Object> GetOrCreateCastPlatformObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> global);

// Returns parent[key] if it is a plain data property holding an object, and
// (re)creates it as a v8::Object otherwise. This will aggressively overwrite
// things that are not plain objects. Author-defined accessors and proxy traps
// are never invoked, so no author script runs.
v8::Local<v8::Object> EnsureObjectExists(v8::Isolate* isolate,
                                         v8::Local<v8::Object> parent,
                                         const std::string& key);

// Binds |method| and |args| into a JS function named |method_name| which will
// be attached to |parent_object|. Return the bound function to caller.
template <typename Functor, typename... Args>
v8::Local<v8::Function> InstallBinding(v8::Isolate* isolate,
                                       v8::Local<v8::Object> parent_object,
                                       std::string method_name,
                                       Functor method,
                                       Args&&... args) {
  v8::Local<v8::FunctionTemplate> temp(gin::CreateFunctionTemplate(
      isolate, base::BindRepeating(method, std::forward<Args>(args)...)));
  v8::Local<v8::Function> func =
      temp->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
  // CreateDataProperty(), unlike Set(), does not invoke author-defined
  // accessors or proxy traps, so no author script runs while bindings are
  // being installed.
  v8::Maybe<bool> result = parent_object->CreateDataProperty(
      isolate->GetCurrentContext(),
      gin::StringToSymbol(isolate, std::move(method_name)), func);
  if (result.IsNothing() || !result.FromJust()) {
    LOG(ERROR) << "Failed to install binding for method " << method_name;
  }

  return func;
}

// Template for managing the lifetime of a cast_shell binding. Derive from
// from CastBinding and the class will be destroyed when the frame is
// destroyed.
class CastBinding : public content::RenderFrameObserver {
 public:
  CastBinding(const CastBinding&) = delete;
  CastBinding& operator=(const CastBinding&) = delete;

  void TryInstall();

 protected:
  explicit CastBinding(content::RenderFrame* render_frame);
  ~CastBinding() override;

  // content::RenderFrameObserver implementation:
  void DidClearWindowObject() final;
  void OnDestruct() final;

  // Adds function bindings to the cast.__platform__ object.
  // The function can be called multiple times, sub classes should make sure
  // the binding is updated in every call.
  virtual void Install(v8::Local<v8::Object> cast_platform,
                       v8::Isolate* isolate) = 0;

 private:
  base::WeakPtrFactory<CastBinding> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_NATIVE_BINDINGS_HELPER_H_
