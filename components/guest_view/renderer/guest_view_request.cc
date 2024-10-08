// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_request.h"

#include <tuple>
#include <utility>

#include "components/guest_view/renderer/guest_view_container.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-microtask-queue.h"

namespace guest_view {

GuestViewAttachRequest::GuestViewAttachRequest(
    guest_view::GuestViewContainer* container,
    content::RenderFrame* render_frame,
    int guest_instance_id,
    base::Value::Dict params,
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate)
    : container_(container),
      callback_(isolate, callback),
      isolate_(isolate),
      guest_instance_id_(guest_instance_id),
      params_(std::move(params)) {
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
}

GuestViewAttachRequest::~GuestViewAttachRequest() = default;

void GuestViewAttachRequest::PerformRequest() {
  remote_->AttachToEmbedderFrame(
      container_->element_instance_id(), guest_instance_id_, params_.Clone(),
      base::BindOnce(&GuestViewAttachRequest::OnAcknowledged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GuestViewAttachRequest::OnAcknowledged() {
  // Destroys `this`.
  container_->OnRequestAcknowledged(this);
}

void GuestViewAttachRequest::ExecuteCallbackIfAvailable(
    int argc,
    std::unique_ptr<v8::Local<v8::Value>[]> argv) {
  if (callback_.IsEmpty())
    return;

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Function> callback =
      v8::Local<v8::Function>::New(isolate_, callback_);
  v8::Local<v8::Context> context;
  if (!callback->GetCreationContext(isolate_).ToLocal(&context)) {
    return;
  }

  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(isolate_, context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  callback->Call(context, context->Global(), argc, argv.get())
      .FromMaybe(v8::Local<v8::Value>());
}

}  // namespace guest_view
