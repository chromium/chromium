// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_request.h"

#include <tuple>
#include <utility>

#include "components/guest_view/common/guest_view_messages.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace guest_view {

GuestViewRequest::GuestViewRequest(GuestViewContainer* container,
                                   v8::Local<v8::Function> callback,
                                   v8::Isolate* isolate)
    : container_(container),
      callback_(isolate, callback),
      isolate_(isolate) {
}

GuestViewRequest::~GuestViewRequest() {
}

void GuestViewRequest::ExecuteCallbackIfAvailable(
    int argc,
    std::unique_ptr<v8::Local<v8::Value>[]> argv) {
  if (callback_.IsEmpty())
    return;

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Function> callback =
      v8::Local<v8::Function>::New(isolate_, callback_);
  v8::Local<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(
      isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);

  callback->Call(context, context->Global(), argc, argv.get())
      .FromMaybe(v8::Local<v8::Value>());
}

}  // namespace guest_view
