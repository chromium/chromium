// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/js_channel_bindings.h"

#include "chromecast/renderer/native_bindings_helper.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {

// These are defined to be a set of objects that provide a postMessage(string)
// method. In turn they forward messages over their mojo channel to the browser.
JsChannelBindings::JsChannelBindings(
    content::RenderFrame* render_frame,
    mojo::PendingReceiver<mojom::JsChannelClient> receiver)
    : content::RenderFrameObserver(render_frame),
      receiver_(this, std::move(receiver)) {}

JsChannelBindings::~JsChannelBindings() = default;

// static
void JsChannelBindings::Create(content::RenderFrame* render_frame) {
  content::RenderThread* render_thread = content::RenderThread::Get();

  // First, get a connection to the main service for our process.
  mojo::PendingRemote<mojom::JsChannelBindingProvider> pending_remote;
  render_thread->BindHostReceiver(
      pending_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::JsChannelBindingProvider> provider(
      std::move(pending_remote));

  mojo::PendingRemote<mojom::JsChannelClient> client;

  // This deletes itself when the RenderFrame is destroyed.
  new JsChannelBindings(render_frame, client.InitWithNewPipeAndPassReceiver());

  // Tell the browser that we are ready to receive pipes.
  provider->Register(render_frame->GetRoutingID(), std::move(client));
}

void JsChannelBindings::DidClearWindowObject() {
  for (auto& e : channels_)
    Install(e.first);
}

void JsChannelBindings::OnDestruct() {
  delete this;
}

void JsChannelBindings::DidCreateScriptContext(v8::Local<v8::Context> context,
                                               int32_t world_id) {
  // World 0 is the "main" world that page JS uses. Non-zero IDs indicate worker
  // threads or context scripts.
  if (world_id == 0 && !did_create_script_context_) {
    did_create_script_context_ = true;
    for (auto& channel : channels_)
      Install(channel.first);
  }
}

void JsChannelBindings::Install(const std::string& channel) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (!web_frame)
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  if (!isolate)
    return;

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  auto container = EnsureObjectExists(isolate, context->Global(), channel);

  InstallBinding(isolate, container, "postMessage", &JsChannelBindings::Func,
                 base::Unretained(this), channel);
}

void JsChannelBindings::Func(const std::string& channel,
                             v8::Local<v8::Value> message) {
  for (auto& e : channels_) {
    if (e.first == channel) {
      v8::String::Utf8Value utf8(blink::MainThreadIsolate(), message);
      e.second->PostMessage(*utf8);
      break;
    }
  }
}

void JsChannelBindings::CreateChannel(
    const std::string& channel,
    mojo::PendingRemote<mojom::JsChannel> pipe) {
  channels_.push_back(
      std::make_pair(channel, mojo::Remote<mojom::JsChannel>(std::move(pipe))));
  if (did_create_script_context_)
    Install(channel);
}

void JsChannelBindings::RemoveChannel(const std::string& channel) {
  for (auto iter = channels_.begin(); iter != channels_.end(); ++iter) {
    if (iter->first == channel) {
      channels_.erase(iter);
      break;
    }
  }

  if (!did_create_script_context_)
    return;

  // Remove V8 object.
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (!web_frame)
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  if (!isolate)
    return;

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  auto result = context->Global()->Set(
      context, gin::StringToSymbol(isolate, channel), v8::Local<v8::Object>());
  if (result.IsNothing() || !result.FromJust())
    VLOG(1) << "Failed to remove binding for method " << channel;
}

}  // namespace chromecast
