// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_hooks_delegate.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

namespace {

void IsInstalledGetterCallback(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::HandleScope handle_scope(info.GetIsolate());
  v8::Local<v8::Context> context = info.Holder()->CreationContext();
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);

  // The ScriptContext may have been invalidated if e.g. the frame was removed.
  // Return undefined in this case.
  if (!script_context)
    return;

  auto* hooks_delegate =
      static_cast<AppHooksDelegate*>(info.Data().As<v8::External>()->Value());
  // Since this is more-or-less an API, log it as an API call.
  APIActivityLogger::LogAPICall(context, "app.getIsInstalled",
                                std::vector<v8::Local<v8::Value>>());
  info.GetReturnValue().Set(hooks_delegate->GetIsInstalled(script_context));
}

}  // namespace

AppHooksDelegate::IPCHelper::IPCHelper(AppHooksDelegate* owner)
    : owner_(owner) {}
AppHooksDelegate::IPCHelper::~IPCHelper() = default;

void AppHooksDelegate::IPCHelper::SendGetAppInstallStateMessage(
    content::RenderFrame* render_frame,
    const GURL& url,
    int request_id) {
  Send(new ExtensionHostMsg_GetAppInstallState(
      render_frame->GetRoutingID(), url, GetRoutingID(), request_id));
}

bool AppHooksDelegate::IPCHelper::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AppHooksDelegate::IPCHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_GetAppInstallStateResponse,
                        OnAppInstallStateResponse)
    IPC_MESSAGE_UNHANDLED(CHECK(false) << "Unhandled IPC message")
  IPC_END_MESSAGE_MAP()
  return true;
}

void AppHooksDelegate::IPCHelper::OnAppInstallStateResponse(
    const std::string& state,
    int request_id) {
  owner_->OnAppInstallStateResponse(state, request_id);
}

AppHooksDelegate::AppHooksDelegate(Dispatcher* dispatcher,
                                   APIRequestHandler* request_handler)
    : dispatcher_(dispatcher),
      request_handler_(request_handler),
      ipc_helper_(this) {}
AppHooksDelegate::~AppHooksDelegate() {}

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
    std::vector<v8::Local<v8::Value>>* arguments,
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
    int request_id = request_handler_->AddPendingRequest(
        context, (*parse_result.arguments)[0].As<v8::Function>());
    GetInstallState(script_context, request_id);
  } else {
    NOTREACHED();
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
  object_template->SetAccessor(gin::StringToSymbol(isolate, "isInstalled"),
                               &IsInstalledGetterCallback, nullptr,
                               v8::External::New(isolate, this));
}

v8::Local<v8::Value> AppHooksDelegate::GetDetails(
    ScriptContext* script_context) const {
  blink::WebLocalFrame* web_frame = script_context->web_frame();
  CHECK(web_frame);

  v8::Isolate* isolate = script_context->isolate();
  if (web_frame->GetDocument().GetSecurityOrigin().IsUnique())
    return v8::Null(isolate);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          web_frame->GetDocument().Url());

  if (!extension)
    return v8::Null(isolate);

  std::unique_ptr<base::DictionaryValue> manifest_copy =
      extension->manifest()->value()->CreateDeepCopy();
  manifest_copy->SetString("id", extension->id());
  return content::V8ValueConverter::Create()->ToV8Value(
      manifest_copy.get(), script_context->v8_context());
}

void AppHooksDelegate::GetInstallState(ScriptContext* script_context,
                                       int request_id) {
  content::RenderFrame* render_frame = script_context->GetRenderFrame();
  CHECK(render_frame);

  ipc_helper_.SendGetAppInstallStateMessage(
      render_frame, script_context->web_frame()->GetDocument().Url(),
      request_id);
}

const char* AppHooksDelegate::GetRunningState(
    ScriptContext* script_context) const {
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

void AppHooksDelegate::OnAppInstallStateResponse(const std::string& state,
                                                 int request_id) {
  // Note: it's kind of lame that we serialize the install state to a
  // base::Value here when we're just going to later convert it to v8, but it's
  // not worth the specialization on APIRequestHandler for this oddball API.
  base::ListValue response;
  response.AppendString(state);
  request_handler_->CompleteRequest(request_id, response, std::string());
}

}  // namespace extensions
