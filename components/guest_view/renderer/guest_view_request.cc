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

GuestViewAttachRequest::GuestViewAttachRequest(
    GuestViewContainer* container,
    int guest_instance_id,
    std::unique_ptr<base::DictionaryValue> params,
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate)
    : GuestViewRequest(container, callback, isolate),
      guest_instance_id_(guest_instance_id),
      params_(std::move(params)) {}

GuestViewAttachRequest::~GuestViewAttachRequest() {
}

void GuestViewAttachRequest::PerformRequest() {
  if (!container()->render_frame())
    return;

  // TODO(wjmaclean): Can this next chunk be removed?
  // Step 1, send the attach params to guest_view/.
  container()->render_frame()->Send(
      new GuestViewHostMsg_AttachGuest(container()->element_instance_id(),
                                       guest_instance_id_,
                                       *params_));
}

void GuestViewAttachRequest::HandleResponse(const IPC::Message& message) {
  // TODO(fsamuel): Rename this message so that it's apparent that this is a
  // response to GuestViewHostMsg_AttachGuest. Perhaps
  // GuestViewMsg_AttachGuest_ACK?
  GuestViewMsg_GuestAttached::Param param;
  if (!GuestViewMsg_GuestAttached::Read(&message, &param))
    return;

  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(std::get<1>(param));
  // TODO(fsamuel): Should we be reporting an error to JavaScript or DCHECKing?
  if (!guest_proxy_render_view)
    return;

  v8::HandleScope handle_scope(isolate());
  blink::WebFrame* frame = guest_proxy_render_view->GetWebView()->MainFrame();
  v8::Local<v8::Value> window = frame->GlobalProxy();

  const int argc = 1;
  std::unique_ptr<v8::Local<v8::Value>[]> argv(new v8::Local<v8::Value>[argc]);
  argv[0] = window;

  // Call the AttachGuest API's callback with the guest proxy as the first
  // parameter.
  ExecuteCallbackIfAvailable(argc, std::move(argv));
}

}  // namespace guest_view
