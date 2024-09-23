// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/js_injection/renderer/js_binding.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/overloaded.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/js_injection/common/interfaces.mojom-forward.h"
#include "components/js_injection/renderer/js_communication.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_message_port_converter.h"
#include "v8/include/v8.h"

namespace {
constexpr char kPostMessage[] = "postMessage";
constexpr char kOnMessage[] = "onmessage";
constexpr char kAddEventListener[] = "addEventListener";
constexpr char kRemoveEventListener[] = "removeEventListener";

class V8ArrayBufferPayload : public blink::WebMessageArrayBufferPayload {
 public:
  explicit V8ArrayBufferPayload(v8::Local<v8::ArrayBuffer> array_buffer)
      : array_buffer_(array_buffer) {
    CHECK(!array_buffer_.IsEmpty());
  }

  // Although resize *may* be supported, it's not needed to be handled for JS to
  // browser messaging.
  bool GetIsResizableByUserJavaScript() const override { return false; }

  size_t GetMaxByteLength() const override { return GetLength(); }

  size_t GetLength() const override { return array_buffer_->ByteLength(); }

  std::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const override {
    return base::make_span(static_cast<const uint8_t*>(array_buffer_->Data()),
                           array_buffer_->ByteLength());
  }

  void CopyInto(base::span<uint8_t> dest) const override {
    CHECK_GE(dest.size(), array_buffer_->ByteLength());
    memcpy(dest.data(), array_buffer_->Data(), array_buffer_->ByteLength());
  }

 private:
  v8::Local<v8::ArrayBuffer> array_buffer_;
};

}  // anonymous namespace

namespace js_injection {

gin::WrapperInfo JsBinding::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
base::WeakPtr<JsBinding> JsBinding::Install(
    content::RenderFrame* render_frame,
    const std::u16string& js_object_name,
    base::WeakPtr<JsCommunication> js_communication,
    v8::Isolate* isolate,
    v8::Local<v8::Context> context) {
  CHECK(!js_object_name.empty())
      << "JavaScript wrapper name shouldn't be empty";

  std::optional<v8::HandleScope> handle_scope;
  std::optional<v8::Context::Scope> context_scope;
  // The scopes may have already been setup outside this method.
  if (!isolate) {
    blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
    isolate = web_frame->GetAgentGroupScheduler()->Isolate();
    handle_scope.emplace(isolate);
    context = web_frame->MainWorldScriptContext();
    if (context.IsEmpty()) {
      return nullptr;
    }

    context_scope.emplace(context);
  }
  // The call to CreateHandle() takes ownership of `js_binding` (but only on
  // success).
  JsBinding* js_binding =
      new JsBinding(render_frame, js_object_name, js_communication);
  gin::Handle<JsBinding> bindings = gin::CreateHandle(isolate, js_binding);
  if (bindings.IsEmpty()) {
    delete js_binding;
    return nullptr;
  }

  v8::Local<v8::Object> global = context->Global();
  global
      ->CreateDataProperty(context,
                           gin::StringToSymbol(isolate, js_object_name),
                           bindings.ToV8())
      .Check();

  return js_binding->weak_ptr_factory_.GetWeakPtr();
}

JsBinding::JsBinding(content::RenderFrame* render_frame,
                     const std::u16string& js_object_name,
                     base::WeakPtr<JsCommunication> js_communication)
    : render_frame_(render_frame),
      js_object_name_(js_object_name),
      js_communication_(js_communication) {
}

JsBinding::~JsBinding() = default;

void JsBinding::OnPostMessage(blink::WebMessagePayload message) {
  // If `js_communication_` is null, this object will soon be destroyed.
  if (!js_communication_)
    return;

  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  if (!web_frame)
    return;
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  // Setting verbose makes the exception get reported to the default
  // uncaught-exception handlers, rather than just being silently swallowed.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Value> v8_message = absl::visit(
      base::Overloaded{
          [isolate](std::u16string& string_value) -> v8::Local<v8::Value> {
            return gin::ConvertToV8(isolate, std::move(string_value));
          },
          [isolate](std::unique_ptr<blink::WebMessageArrayBufferPayload>&
                        array_buffer_value) -> v8::Local<v8::Value> {
            auto backing_store = v8::ArrayBuffer::NewBackingStore(
                isolate, array_buffer_value->GetLength());
            CHECK(backing_store->ByteLength() ==
                  array_buffer_value->GetLength());
            array_buffer_value->CopyInto(
                base::make_span(static_cast<uint8_t*>(backing_store->Data()),
                                backing_store->ByteLength()));
            return v8::ArrayBuffer::New(isolate, std::move(backing_store));
          }},
      message);

  // Simulate MessageEvent's data property. See
  // https://html.spec.whatwg.org/multipage/comms.html#messageevent
  v8::Local<v8::Object> event =
      gin::DataObjectBuilder(isolate).Set("data", v8_message).Build();
  v8::Local<v8::Value> argv[] = {event};

  v8::Local<v8::Object> self = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Function> on_message = GetOnMessage(isolate);
  if (!on_message.IsEmpty()) {
    web_frame->RequestExecuteV8Function(context, on_message, self, 1, argv, {});
  }

  // Copy the listeners so that if the listener modifies the list in some way
  // there isn't a UAF.
  v8::LocalVector<v8::Function> listeners_copy(isolate);
  listeners_copy.reserve(listeners_.size());
  for (const auto& listener : listeners_) {
    listeners_copy.push_back(listener.Get(isolate));
  }
  for (const auto& listener : listeners_copy) {
    // Ensure the listener is still registered.
    if (base::Contains(listeners_, listener)) {
      web_frame->RequestExecuteV8Function(context, listener, self, 1, argv, {});
    }
  }
}

void JsBinding::ReleaseV8GlobalObjects() {
  listeners_.clear();
  on_message_.Reset();
}

void JsBinding::Bind(
    mojo::PendingAssociatedReceiver<mojom::BrowserToJsMessaging> receiver) {
  receiver_.reset();
  return receiver_.Bind(std::move(receiver));
}

gin::ObjectTemplateBuilder JsBinding::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<JsBinding>::GetObjectTemplateBuilder(isolate)
      .SetMethod(kPostMessage, &JsBinding::PostMessage)
      .SetMethod(kAddEventListener, &JsBinding::AddEventListener)
      .SetMethod(kRemoveEventListener, &JsBinding::RemoveEventListener)
      .SetProperty(kOnMessage, &JsBinding::GetOnMessage,
                   &JsBinding::SetOnMessage);
}

void JsBinding::PostMessage(gin::Arguments* args) {
  v8::Local<v8::Value> js_payload;
  if (!args->GetNext(&js_payload)) {
    args->ThrowError();
    return;
  }
  blink::WebMessagePayload message_payload;
  if (js_payload->IsString()) {
    std::u16string string;
    gin::Converter<std::u16string>::FromV8(args->isolate(), js_payload,
                                           &string);
    message_payload = std::move(string);
  } else if (js_payload->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> array_buffer = js_payload.As<v8::ArrayBuffer>();
    message_payload = std::make_unique<V8ArrayBufferPayload>(array_buffer);
  } else {
    args->ThrowError();
    return;
  }

  std::vector<blink::MessagePortChannel> ports;
  v8::LocalVector<v8::Object> objs(args->isolate());
  // If we get more than two arguments and the second argument is not an array
  // of ports, we can't process.
  if (args->Length() >= 2 && !args->GetNext(&objs)) {
    args->ThrowError();
    return;
  }

  for (auto& obj : objs) {
    std::optional<blink::MessagePortChannel> port =
        blink::WebMessagePortConverter::DisentangleAndExtractMessagePortChannel(
            args->isolate(), obj);
    // If the port is null we should throw an exception.
    if (!port.has_value()) {
      args->ThrowError();
      return;
    }
    ports.emplace_back(port.value());
  }

  mojom::JsToBrowserMessaging* js_to_java_messaging =
      js_communication_ ? js_communication_->GetJsToJavaMessage(js_object_name_)
                        : nullptr;
  if (js_to_java_messaging) {
    js_to_java_messaging->PostMessage(
        std::move(message_payload),
        blink::MessagePortChannel::ReleaseHandles(ports));
  }
}

// AddEventListener() needs to match EventTarget's AddEventListener() in blink.
// It takes |type|, |listener| parameters, we ignore the |options| parameter.
// See https://dom.spec.whatwg.org/#dom-eventtarget-addeventlistener
void JsBinding::AddEventListener(gin::Arguments* args) {
  std::string type;
  if (!args->GetNext(&type)) {
    args->ThrowError();
    return;
  }

  // We only support message event.
  if (type != "message")
    return;

  v8::Local<v8::Function> listener;
  if (!args->GetNext(&listener))
    return;

  // Should be at most 3 parameters.
  if (args->Length() > 3) {
    args->ThrowError();
    return;
  }

  if (base::Contains(listeners_, listener))
    return;

  v8::Local<v8::Context> context = args->GetHolderCreationContext();
  listeners_.push_back(
      v8::Global<v8::Function>(context->GetIsolate(), listener));
}

// RemoveEventListener() needs to match EventTarget's RemoveEventListener() in
// blink. It takes |type|, |listener| parameters, we ignore |options| parameter.
// See https://dom.spec.whatwg.org/#dom-eventtarget-removeeventlistener
void JsBinding::RemoveEventListener(gin::Arguments* args) {
  std::string type;
  if (!args->GetNext(&type)) {
    args->ThrowError();
    return;
  }

  // We only support message event.
  if (type != "message")
    return;

  v8::Local<v8::Function> listener;
  if (!args->GetNext(&listener))
    return;

  // Should be at most 3 parameters.
  if (args->Length() > 3) {
    args->ThrowError();
    return;
  }

  auto iter = base::ranges::find(listeners_, listener);
  if (iter == listeners_.end())
    return;

  listeners_.erase(iter);
}

v8::Local<v8::Function> JsBinding::GetOnMessage(v8::Isolate* isolate) {
  return on_message_.Get(isolate);
}

void JsBinding::SetOnMessage(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value->IsFunction())
    on_message_.Reset(isolate, value.As<v8::Function>());
  else
    on_message_.Reset();
}

}  // namespace js_injection
