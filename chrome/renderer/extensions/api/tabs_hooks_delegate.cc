// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/tabs_hooks_delegate.h"

#include <string_view>

#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"

namespace extensions {

namespace {

using RequestResult = APIBindingHooks::RequestResult;

constexpr char kConnect[] = "tabs.connect";
constexpr char kSendMessage[] = "tabs.sendMessage";
constexpr char kSendTabsRequest[] = "tabs.sendRequest";

}  // namespace

TabsHooksDelegate::TabsHooksDelegate(
    NativeRendererMessagingService* messaging_service)
    : messaging_service_(messaging_service) {}
TabsHooksDelegate::~TabsHooksDelegate() {}

RequestResult TabsHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  // TODO(devlin): This logic is the same in the RuntimeCustomHooksDelegate -
  // would it make sense to share it?
  using Handler = RequestResult (TabsHooksDelegate::*)(
      ScriptContext*, const APISignature::V8ParseResult&);
  static const struct {
    Handler handler;
    std::string_view method;
  } kHandlers[] = {
      {&TabsHooksDelegate::HandleSendMessage, kSendMessage},
      {&TabsHooksDelegate::HandleSendRequest, kSendTabsRequest},
      {&TabsHooksDelegate::HandleConnect, kConnect},
  };

  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler)
    return RequestResult(RequestResult::NOT_HANDLED);

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }

  return (this->*handler)(script_context, parse_result);
}

RequestResult TabsHooksDelegate::HandleSendRequest(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(3u, arguments.size());
  // tabs.sendRequest() is restricted to MV2, so it should never be called with
  // a promise based request as they are restricted to MV3 and above.
  DCHECK_NE(binding::AsyncResponseType::kPromise, parse_result.async_type);

  int tab_id = messaging_util::ExtractIntegerId(arguments[0]);
  v8::Local<v8::Value> v8_message = arguments[1];
  std::string error;

  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      script_context->v8_context(), v8_message,
      messaging_util::GetSerializationFormat(*script_context), &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[2]->IsNull())
    response_callback = arguments[2].As<v8::Function>();

  messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForTab(tab_id, messaging_util::kNoFrameId),
      mojom::ChannelType::kSendRequest, *message, parse_result.async_type,
      response_callback);

  return RequestResult(RequestResult::HANDLED);
}

RequestResult TabsHooksDelegate::HandleSendMessage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(4u, arguments.size());

  int tab_id = messaging_util::ExtractIntegerId(arguments[0]);
  messaging_util::MessageOptions options;
  if (!arguments[2]->IsNull()) {
    options = messaging_util::ParseMessageOptions(
        script_context->v8_context(), arguments[2].As<v8::Object>(),
        messaging_util::PARSE_FRAME_ID);
  }

  v8::Local<v8::Value> v8_message = arguments[1];
  DCHECK(!v8_message.IsEmpty());
  std::string error;

  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      script_context->v8_context(), v8_message,
      messaging_util::GetSerializationFormat(*script_context), &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[3]->IsNull())
    response_callback = arguments[3].As<v8::Function>();

  v8::Local<v8::Promise> promise = messaging_service_->SendOneTimeMessage(
      script_context,
      MessageTarget::ForTab(tab_id, options.frame_id, options.document_id),
      mojom::ChannelType::kSendMessage, *message, parse_result.async_type,
      response_callback);
  DCHECK_EQ(parse_result.async_type == binding::AsyncResponseType::kPromise,
            !promise.IsEmpty())
      << "SendOneTimeMessage should only return a Promise for promise based "
         "API calls, otherwise it should be empty";

  RequestResult result(RequestResult::HANDLED);
  if (parse_result.async_type == binding::AsyncResponseType::kPromise) {
    result.return_value = promise;
  }
  return result;
}

RequestResult TabsHooksDelegate::HandleConnect(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(2u, arguments.size());
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);

  int tab_id = messaging_util::ExtractIntegerId(arguments[0]);

  messaging_util::MessageOptions options;
  if (!arguments[1]->IsNull()) {
    options = messaging_util::ParseMessageOptions(
        script_context->v8_context(), arguments[1].As<v8::Object>(),
        messaging_util::PARSE_FRAME_ID | messaging_util::PARSE_CHANNEL_NAME);
  }

  gin::Handle<GinPort> port = messaging_service_->Connect(
      script_context,
      MessageTarget::ForTab(tab_id, options.frame_id, options.document_id),
      options.channel_name,
      messaging_util::GetSerializationFormat(*script_context));
  DCHECK(!port.IsEmpty());

  RequestResult result(RequestResult::HANDLED);
  result.return_value = port.ToV8();
  return result;
}

}  // namespace extensions
