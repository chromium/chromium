// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/password_manager/remote_actor_credential_sharing_extension.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace {

bool ShouldExposeRemoteActorCredentialSharingJavascriptApi(
    content::RenderFrame* render_frame) {
  if (!render_frame || !render_frame->IsMainFrame() ||
      render_frame->IsInFencedFrameTree()) {
    return false;
  }

  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  // Site isolation verification: Ensure the frame is locked to its site.
  if (!blink::Platform::Current()->IsLockedToSite()) {
    return false;
  }

  // Verify that the frame's security origin is allowed.
  url::Origin origin = url::Origin(web_frame->GetSecurityOrigin());
  return password_manager::IsRemoteActorCredentialSharingAllowedForOrigin(
      origin);
}

}  // namespace

// static
void RemoteActorCredentialSharingExtension::Create(
    content::RenderFrame* render_frame) {
  new RemoteActorCredentialSharingExtension(render_frame);
}

RemoteActorCredentialSharingExtension::RemoteActorCredentialSharingExtension(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

RemoteActorCredentialSharingExtension::
    ~RemoteActorCredentialSharingExtension() = default;

void RemoteActorCredentialSharingExtension::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeRemoteActorCredentialSharingJavascriptApi(render_frame())) {
    Install(context);
  }
}

void RemoteActorCredentialSharingExtension::OnDestruct() {
  delete this;
}

void RemoteActorCredentialSharingExtension::Install(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);

  // Bind the requestAgentAuthentication JavaScript method to the C++
  // method templates.
  std::ignore = chrome->Set(
      context, gin::StringToV8(isolate, "requestAgentAuthentication"),
      gin::CreateFunctionTemplate(
          isolate, base::BindRepeating(&RemoteActorCredentialSharingExtension::
                                           RequestAgentAuthentication,
                                       weak_ptr_factory_.GetWeakPtr()))
          ->GetFunction(context)
          .ToLocalChecked());
}

void RemoteActorCredentialSharingExtension::RequestAgentAuthentication(
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& remote_actor_id,
    v8::Local<v8::Function> callback_function) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(isolate, callback_function);

  // Require a transient user gesture to execute the Mojo API call.
  if (!web_frame->HasTransientUserActivation()) {
    base::BindOnce(
        &RemoteActorCredentialSharingExtension::RunCompletionCallback,
        weak_ptr_factory_.GetWeakPtr(), std::move(global_callback),
        /*success=*/false)
        .Run();
    return;
  }

  GetRemoteInterface().RequestAgentAuthentication(
      gaia_id, domain, remote_actor_id,
      base::BindOnce(
          &RemoteActorCredentialSharingExtension::RunCompletionCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(global_callback)));
}

void RemoteActorCredentialSharingExtension::RunCompletionCallback(
    std::unique_ptr<v8::Global<v8::Function>> callback,
    bool success) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Local<v8::Function> callback_local =
      v8::Local<v8::Function>::New(isolate, *callback);
  if (callback_local.IsEmpty()) {
    return;
  }

  // Security hardening: Verify the callback's creation context is still the
  // current main world script context before executing it. This prevents
  // context leakage (UXSS) or crashes if navigation occurred.
  v8::Local<v8::Context> callback_context;
  if (!callback_local->GetCreationContext(isolate).ToLocal(&callback_context) ||
      callback_context != context) {
    return;
  }

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> argv[] = {v8::Boolean::New(isolate, success)};
  web_frame->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Undefined(isolate), std::size(argv), argv);
}

chrome::mojom::RemoteActorCredentialSharing&
RemoteActorCredentialSharingExtension::GetRemoteInterface() {
  if (!remote_interface_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        remote_interface_.BindNewEndpointAndPassReceiver());
  }
  return *remote_interface_.get();
}
