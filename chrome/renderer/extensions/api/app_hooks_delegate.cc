// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/app_hooks_delegate.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

namespace {

void EmptySetterCallback(v8::Local<v8::Name> name,
                         v8::Local<v8::Value> value,
                         const v8::PropertyCallbackInfo<void>& info) {
  // Empty setter is required to keep the native data property in "accessor"
  // state even in case the value is updated by user code.
}

}  // namespace

// static
void AppHooksDelegate::IsInstalledGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::HandleScope handle_scope(info.GetIsolate());
  v8::Local<v8::Context> context =
      info.Holder()->GetCreationContextChecked(info.GetIsolate());
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);

  // The ScriptContext may have been invalidated if e.g. the frame was removed.
  // Return undefined in this case.
  if (!script_context)
    return;

  auto* hooks_delegate =
      static_cast<AppHooksDelegate*>(info.Data().As<v8::External>()->Value());
  // Since this is more-or-less an API, log it as an API call.
  APIActivityLogger::LogAPICall(hooks_delegate->ipc_sender_, context,
                                "app.getIsInstalled",
                                v8::LocalVector<v8::Value>(info.GetIsolate()));
  info.GetReturnValue().Set(hooks_delegate->GetIsInstalled(script_context));
}

AppHooksDelegate::AppHooksDelegate(Dispatcher* dispatcher,
                                   APIRequestHandler* request_handler,
                                   IPCMessageSender* ipc_sender)
    : dispatcher_(dispatcher),
      request_handler_(request_handler),
      ipc_sender_(ipc_sender) {}
AppHooksDelegate::~AppHooksDelegate() = default;

bool AppHooksDelegate::GetIsInstalled(ScriptContext* script_context) const {
  const Extension* extension = script_context->extension();

  // TODO(aa): Why only hosted app?
  return extension && extension->is_hosted_app() &&
         dispatcher_->IsExtensionActive(extension->id());
}

APIBindingHooks::RequestResult AppHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  using RequestResult = APIBindingHooks::RequestResult;

  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
    return RequestResult(RequestResult::INVALID_INVOCATION);
  }

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  DCHECK(script_context);

  APIBindingHooks::RequestResult result(
      APIBindingHooks::RequestResult::HANDLED);
  if (method_name == "app.getIsInstalled") {
    result.return_value =
        v8::Boolean::New(isolate, GetIsInstalled(script_context));
  } else if (method_name == "app.getDetails") {
    result.return_value = GetDetails(script_context);
  } else if (method_name == "app.runningState") {
    result.return_value =
        gin::StringToSymbol(isolate, GetRunningState(script_context));
  } else if (method_name == "app.installState") {
    DCHECK_EQ(1u, parse_result.arguments->size());
    DCHECK((*parse_result.arguments)[0]->IsFunction());
    APIRequestHandler::RequestDetails request_details =
        request_handler_->AddPendingRequest(
            context, binding::AsyncResponseType::kCallback,
            (*parse_result.arguments)[0].As<v8::Function>(),
            binding::ResultModifierFunction());
    GetInstallState(script_context, request_details.request_id);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  return result;
}

void AppHooksDelegate::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  // We expose a boolean isInstalled on the chrome.app API object, as well as
  // the getIsInstalled() method.
  // TODO(devlin): :(. Hopefully we can just remove this whole API, but this is
  // particularly silly.
  // This object should outlive contexts, so the |this| v8::External is safe.
  // TODO(devlin): This is getting pretty common. We should find a generalized
  // solution, or make gin::ObjectTemplateBuilder work for these use cases.
  object_template->SetNativeDataProperty(
      gin::StringToSymbol(isolate, "isInstalled"),
      &AppHooksDelegate::IsInstalledGetterCallback, EmptySetterCallback,
      v8::External::New(isolate, this));
}

v8::Local<v8::Value> AppHooksDelegate::GetDetails(
    ScriptContext* script_context) const {
  blink::WebLocalFrame* web_frame = script_context->web_frame();
  CHECK(web_frame);

  v8::Isolate* isolate = script_context->isolate();
  if (web_frame->GetDocument().GetSecurityOrigin().IsOpaque())
    return v8::Null(isolate);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          web_frame->GetDocument().Url());

  if (!extension)
    return v8::Null(isolate);

  base::Value::Dict manifest_copy = extension->manifest()->value()->Clone();
  manifest_copy.Set("id", extension->id());
  return content::V8ValueConverter::Create()->ToV8Value(
      manifest_copy, script_context->v8_context());
}

void AppHooksDelegate::GetInstallState(ScriptContext* script_context,
                                       int request_id) {
  content::RenderFrame* render_frame = script_context->GetRenderFrame();
  CHECK(render_frame);

  ExtensionFrameHelper::Get(render_frame)
      ->GetLocalFrameHost()
      ->GetAppInstallState(
          script_context->web_frame()->GetDocument().Url(),
          base::BindOnce(&AppHooksDelegate::OnAppInstallStateResponse,
                         weak_factory_.GetWeakPtr(), request_id));
}

const char* AppHooksDelegate::GetRunningState(
    ScriptContext* script_context) const {
  // If we are in a fenced frame tree then the top security origin
  // does not make sense to look at.
  if (script_context->web_frame()->IsInFencedFrameTree())
    return extension_misc::kAppStateCannotRun;

  // To distinguish between ready_to_run and cannot_run states, we need the app
  // from the top frame.
  const RendererExtensionRegistry* extensions =
      RendererExtensionRegistry::Get();

  url::Origin top_origin =
      script_context->web_frame()->Top()->GetSecurityOrigin();
  // The app associated with the top level frame.
  const Extension* top_app = extensions->GetHostedAppByURL(top_origin.GetURL());

  // The app associated with this frame.
  const Extension* this_app = extensions->GetHostedAppByURL(
      script_context->web_frame()->GetDocument().Url());

  if (!this_app || !top_app)
    return extension_misc::kAppStateCannotRun;

  const char* state = nullptr;
  if (dispatcher_->IsExtensionActive(top_app->id())) {
    if (top_app == this_app)
      state = extension_misc::kAppStateRunning;
    else
      state = extension_misc::kAppStateCannotRun;
  } else if (top_app == this_app) {
    state = extension_misc::kAppStateReadyToRun;
  } else {
    state = extension_misc::kAppStateCannotRun;
  }

  return state;
}

void AppHooksDelegate::OnAppInstallStateResponse(int request_id,
                                                 const std::string& state) {
  // Note: it's kind of lame that we serialize the install state to a
  // base::Value here when we're just going to later convert it to v8, but it's
  // not worth the specialization on APIRequestHandler for this oddball API.
  base::Value::List response;
  response.Append(state);
  request_handler_->CompleteRequest(request_id, response, std::string());
}

}  // namespace extensions
