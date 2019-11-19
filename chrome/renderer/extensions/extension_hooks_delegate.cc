// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_hooks_delegate.h"

#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/view_type.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/runtime_hooks_delegate.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "gin/dictionary.h"

namespace extensions {

namespace {

using RequestResult = APIBindingHooks::RequestResult;

constexpr char kSendExtensionRequest[] = "extension.sendRequest";
constexpr char kGetURL[] = "extension.getURL";
constexpr char kGetBackgroundPage[] = "extension.getBackgroundPage";
constexpr char kGetViews[] = "extension.getViews";
constexpr char kGetExtensionTabs[] = "extension.getExtensionTabs";

// We alias a bunch of chrome.extension APIs to their chrome.runtime
// counterparts.
// NOTE(devlin): This is a very simple alias, in which we just return the
// runtime version from the chrome.runtime object. This is important to note
// for a few reasons:
// - Modifications to the chrome.runtime object will affect the return result
//   here. i.e., if script does chrome.runtime.sendMessage = 'some string',
//   then chrome.extension.sendMessage will also be 'some string'.
// - Events will share listeners. i.e., a listener added to
//   chrome.extension.onMessage will fire from a runtime.onMessage event, and
//   vice versa.
// All of these APIs have been deprecated, and are no longer even documented,
// but still have usage. This is the cheap workaround that JS bindings have been
// using, and, while not robust, it should be secure, so use it native bindings,
// too.
void GetAliasedFeature(v8::Local<v8::Name> property_name,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> chrome;
  if (!context->Global()
           ->Get(context, gin::StringToSymbol(isolate, "chrome"))
           .ToLocal(&chrome) ||
      !chrome->IsObject()) {
    return;
  }

  v8::Local<v8::Value> runtime;
  if (!chrome.As<v8::Object>()
           ->Get(context, gin::StringToSymbol(isolate, "runtime"))
           .ToLocal(&runtime) ||
      !runtime->IsObject()) {
    return;
  }

  v8::Local<v8::Object> runtime_obj = runtime.As<v8::Object>();
  v8::Maybe<bool> has_property =
      runtime_obj->HasRealNamedProperty(context, property_name);
  if (!has_property.IsJust() || !has_property.FromJust())
    return;

  v8::Local<v8::Value> property_value;
  // Try and grab the chrome.runtime version. It's possible this has been
  // tampered with, so early-out if an exception is thrown.
  if (!runtime_obj->Get(context, property_name).ToLocal(&property_value))
    return;

  info.GetReturnValue().Set(property_value);
}

// A helper method to throw a deprecation error on access.
void ThrowDeprecatedAccessError(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  static constexpr char kError[] =
      "extension.sendRequest, extension.onRequest, and "
      "extension.onRequestExternal are deprecated. Please use "
      "runtime.sendMessage, runtime.onMessage, and runtime.onMessageExternal "
      "instead.";
  v8::Isolate* isolate = info.GetIsolate();
  isolate->ThrowException(
      v8::Exception::Error(gin::StringToV8(isolate, kError)));
}

}  // namespace

ExtensionHooksDelegate::ExtensionHooksDelegate(
    NativeRendererMessagingService* messaging_service)
    : messaging_service_(messaging_service) {}
ExtensionHooksDelegate::~ExtensionHooksDelegate() {}

RequestResult ExtensionHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  // TODO(devlin): This logic is the same in the RuntimeCustomHooksDelegate -
  // would it make sense to share it?
  using Handler = RequestResult (ExtensionHooksDelegate::*)(
      ScriptContext*, const std::vector<v8::Local<v8::Value>>&);
  static struct {
    Handler handler;
    base::StringPiece method;
  } kHandlers[] = {
      {&ExtensionHooksDelegate::HandleSendRequest, kSendExtensionRequest},
      {&ExtensionHooksDelegate::HandleGetURL, kGetURL},
      {&ExtensionHooksDelegate::HandleGetBackgroundPage, kGetBackgroundPage},
      {&ExtensionHooksDelegate::HandleGetExtensionTabs, kGetExtensionTabs},
      {&ExtensionHooksDelegate::HandleGetViews, kGetViews},
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

  if (method_name == kSendExtensionRequest) {
    messaging_util::MassageSendMessageArguments(context->GetIsolate(), false,
                                                arguments);
  }

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }

  return (this->*handler)(script_context, *parse_result.arguments);
}

void ExtensionHooksDelegate::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  static constexpr const char* kAliases[] = {
      "connect",   "connectNative",     "sendMessage", "sendNativeMessage",
      "onConnect", "onConnectExternal", "onMessage",   "onMessageExternal",
  };

  for (const auto* alias : kAliases) {
    object_template->SetAccessor(gin::StringToSymbol(isolate, alias),
                                 &GetAliasedFeature);
  }

  bool is_incognito = ExtensionsRendererClient::Get()->IsIncognitoProcess();
  object_template->Set(isolate, "inIncognitoContext",
                       v8::Boolean::New(isolate, is_incognito));
}

void ExtensionHooksDelegate::InitializeInstance(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> instance) {
  // Throw access errors for deprecated sendRequest-related properties. This
  // isn't terribly efficient, but is only done for certain unpacked extensions
  // and only if they access the chrome.extension module.
  if (messaging_util::IsSendRequestDisabled(
          GetScriptContextFromV8ContextChecked(context))) {
    static constexpr const char* kDeprecatedSendRequestProperties[] = {
        "sendRequest", "onRequest", "onRequestExternal"};
    for (const char* property : kDeprecatedSendRequestProperties) {
      v8::Maybe<bool> success = instance->SetAccessor(
          context, gin::StringToV8(context->GetIsolate(), property),
          &ThrowDeprecatedAccessError);
      DCHECK(success.IsJust());
      DCHECK(success.FromJust());
    }
  }
}

RequestResult ExtensionHooksDelegate::HandleSendRequest(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(3u, arguments.size());
  // This DCHECK() is correct because no context with sendRequest-related
  // APIs disabled should have scriptable access to a context with them
  // enabled.
  DCHECK(!messaging_util::IsSendRequestDisabled(script_context));

  std::string target_id;
  std::string error;
  if (!messaging_util::GetTargetExtensionId(script_context, arguments[0],
                                            "extension.sendRequest", &target_id,
                                            &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Value> v8_message = arguments[1];
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      script_context->v8_context(), v8_message, &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[2]->IsNull())
    response_callback = arguments[2].As<v8::Function>();

  messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForExtension(target_id),
      messaging_util::kSendRequestChannel, false, *message, response_callback);

  return RequestResult(RequestResult::HANDLED);
}

RequestResult ExtensionHooksDelegate::HandleGetURL(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  // We call a static implementation here rather using an alias due to not being
  // able to remove the extension.json GetURL entry, as it is used for generated
  // documentation and api feature lists some other methods refer to.
  return RuntimeHooksDelegate::GetURL(script_context, arguments);
}

APIBindingHooks::RequestResult ExtensionHooksDelegate::HandleGetViews(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  const Extension* extension = script_context->extension();
  DCHECK(extension);

  ViewType view_type = VIEW_TYPE_INVALID;
  int window_id = extension_misc::kUnknownWindowId;
  int tab_id = extension_misc::kUnknownTabId;

  if (!arguments[0]->IsNull()) {
    gin::Dictionary options_dict(script_context->isolate(),
                                 arguments[0].As<v8::Object>());
    v8::Local<v8::Value> v8_window_id;
    v8::Local<v8::Value> v8_tab_id;
    v8::Local<v8::Value> v8_view_type;
    if (!options_dict.Get("windowId", &v8_window_id) ||
        !options_dict.Get("tabId", &v8_tab_id) ||
        !options_dict.Get("type", &v8_view_type)) {
      NOTREACHED()
          << "Unexpected exception: argument parsing produces plain objects";
      return RequestResult(RequestResult::THROWN);
    }

    if (!v8_window_id->IsUndefined()) {
      DCHECK(v8_window_id->IsInt32());
      window_id = v8_window_id.As<v8::Int32>()->Value();
    }

    if (!v8_tab_id->IsUndefined()) {
      DCHECK(v8_tab_id->IsInt32());
      tab_id = v8_tab_id.As<v8::Int32>()->Value();
    }

    if (!v8_view_type->IsUndefined()) {
      DCHECK(v8_view_type->IsString());
      std::string view_type_string = base::ToUpperASCII(
          gin::V8ToString(script_context->isolate(), v8_view_type));
      if (view_type_string != "ALL") {
        bool success = GetViewTypeFromString(view_type_string, &view_type);
        DCHECK(success);
      }
    }
  }

  RequestResult result(RequestResult::HANDLED);
  result.return_value = ExtensionFrameHelper::GetV8MainFrames(
      script_context->v8_context(), extension->id(), window_id, tab_id,
      view_type);
  return result;
}

RequestResult ExtensionHooksDelegate::HandleGetExtensionTabs(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  const Extension* extension = script_context->extension();
  DCHECK(extension);

  ViewType view_type = VIEW_TYPE_TAB_CONTENTS;
  int window_id = extension_misc::kUnknownWindowId;
  int tab_id = extension_misc::kUnknownTabId;

  if (!arguments[0]->IsNull())
    window_id = arguments[0].As<v8::Int32>()->Value();

  RequestResult result(RequestResult::HANDLED);
  result.return_value = ExtensionFrameHelper::GetV8MainFrames(
      script_context->v8_context(), extension->id(), window_id, tab_id,
      view_type);
  return result;
}

RequestResult ExtensionHooksDelegate::HandleGetBackgroundPage(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  const Extension* extension = script_context->extension();
  DCHECK(extension);

  RequestResult result(RequestResult::HANDLED);
  result.return_value = ExtensionFrameHelper::GetV8BackgroundPageMainFrame(
      script_context->isolate(), extension->id());
  return result;
}

}  // namespace extensions
