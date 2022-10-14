// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_request.h"

#include <tuple>
#include <utility>

#include "base/no_destructor.h"
#include "components/guest_view/common/guest_view.mojom.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-microtask-queue.h"

namespace guest_view {

namespace {

mojom::GuestViewHost* GetGuestViewHost() {
  static base::NoDestructor<mojo::AssociatedRemote<mojom::GuestViewHost>>
      guest_view_host;
  if (!*guest_view_host) {
    content::RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        guest_view_host.get());
  }

  return guest_view_host->get();
}

}  // namespace

GuestViewAttachRequest::GuestViewAttachRequest(
    guest_view::GuestViewContainer* container,
    int render_frame_routing_id,
    int guest_instance_id,
    base::Value::Dict params,
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate)
    : container_(container),
      callback_(isolate, callback),
      isolate_(isolate),
      render_frame_routing_id_(render_frame_routing_id),
      guest_instance_id_(guest_instance_id),
      params_(std::move(params)) {}

GuestViewAttachRequest::~GuestViewAttachRequest() = default;

void GuestViewAttachRequest::PerformRequest() {
  GetGuestViewHost()->AttachToEmbedderFrame(
      render_frame_routing_id_, container_->element_instance_id(),
      guest_instance_id_, params_.Clone(),
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
  if (!callback->GetCreationContext().ToLocal(&context))
    return;

  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(isolate_, context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  callback->Call(context, context->Global(), argc, argv.get())
      .FromMaybe(v8::Local<v8::Value>());
}

}  // namespace guest_view
