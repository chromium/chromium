// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/assistant_bindings.h"

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {
namespace shell {

namespace {

const int kMaxMessageQueueSize = 50;
static const int64_t kDelayBetweenReconnectionInMillis = 100;

const char kSetAssistantMessageHandlerMethodName[] =
    "setAssistantMessageHandler";
const char kSendAssistantRequestMethodName[] = "sendAssistantRequest";

}  // namespace

AssistantBindings::AssistantBindings(content::RenderFrame* frame,
                                     const base::Value::Dict& feature_config)
    : CastBinding(frame),
      feature_config_(feature_config.Clone()),
      message_client_binding_(this),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

AssistantBindings::~AssistantBindings() {}

void AssistantBindings::OnMessage(base::Value message) {
  if (assistant_message_handler_.IsEmpty()) {
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Function> handler = v8::Local<v8::Function>::New(
      isolate, std::move(assistant_message_handler_));

  std::string json;
  base::JSONWriter::Write(message, &json);
  v8::Local<v8::Value> message_val =
      gin::Converter<std::string>::ToV8(isolate, json);

  v8::Local<v8::Value> argv[] = {message_val};
  web_frame->CallFunctionEvenIfScriptDisabled(handler, context->Global(),
                                              std::size(argv), argv);

  assistant_message_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, handler);
}

void AssistantBindings::Install(v8::Local<v8::Object> cast_platform,
                                v8::Isolate* isolate) {
  DVLOG(1) << "Installing AssistantBindings";

  InstallBinding(isolate, cast_platform, kSetAssistantMessageHandlerMethodName,
                 &AssistantBindings::SetAssistantMessageHandler,
                 base::Unretained(this));
  InstallBinding(isolate, cast_platform, kSendAssistantRequestMethodName,
                 &AssistantBindings::SendAssistantRequest,
                 base::Unretained(this));
}

void AssistantBindings::SetAssistantMessageHandler(
    v8::Local<v8::Function> assistant_message_handler) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  assistant_message_handler_ =
      v8::UniquePersistent<v8::Function>(isolate, assistant_message_handler);
  ReconnectMessagePipe();
}

void AssistantBindings::SendAssistantRequest(const std::string& request) {
  if (assistant_message_handler_.IsEmpty()) {
    v8::Isolate* isolate =
        render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
    isolate->ThrowException(
        v8::String::NewFromUtf8(isolate,
                                "Error: assistant message handler is not set.",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked());
    return;
  }
  if (!message_pipe_.is_bound()) {
    if (v8_to_assistant_queue_.size() < kMaxMessageQueueSize) {
      v8_to_assistant_queue_.push_back(request);
    } else {
      LOG(WARNING) << "Messages sending to assistant overflow, will drop "
                      "upcoming messages";
    }
    return;
  }

  v8_to_assistant_queue_.push_back(request);
  FlushV8ToAssistantQueue();
}

void AssistantBindings::ReconnectMessagePipe() {
  if (message_client_binding_.is_bound())
    message_client_binding_.reset();
  if (message_pipe_.is_bound())
    message_pipe_.reset();
  LOG(INFO) << "Creating message pipe";
  const std::string* app_id = feature_config_.FindString("app_id");
  DCHECK(app_id) << "Couldn't get app_id from feature config";
  GetMojoInterface()->CreateMessagePipe(
      *app_id, message_client_binding_.BindNewPipeAndPassRemote(),
      message_pipe_.BindNewPipeAndPassReceiver());

  reconnect_assistant_timer_.Stop();
}

void AssistantBindings::OnAssistantConnectionError() {
  LOG(WARNING) << "Disconnected from assistant. Will reconnect every "
               << kDelayBetweenReconnectionInMillis << " milliseconds";
  assistant_.reset();
  reconnect_assistant_timer_.Start(
      FROM_HERE, base::Milliseconds(kDelayBetweenReconnectionInMillis), this,
      &AssistantBindings::ReconnectMessagePipe);
}

void AssistantBindings::FlushV8ToAssistantQueue() {
  DCHECK(message_pipe_.is_bound());

  for (auto& request : v8_to_assistant_queue_) {
    auto value = base::JSONReader::Read(request);
    if (!value) {
      LOG(ERROR) << "Unable to parse Assistant message JSON.";
      continue;
    }
    message_pipe_->SendMessage(std::move(*value));
  }
  v8_to_assistant_queue_.clear();
}

const mojo::Remote<chromecast::mojom::AssistantMessageService>&
AssistantBindings::GetMojoInterface() {
  if (!assistant_.is_bound()) {
    render_frame()->GetBrowserInterfaceBroker().GetInterface(
        assistant_.BindNewPipeAndPassReceiver());
    assistant_.set_disconnect_handler(base::BindOnce(
        &AssistantBindings::OnAssistantConnectionError, weak_this_));
  }
  return assistant_;
}

}  // namespace shell
}  // namespace chromecast
