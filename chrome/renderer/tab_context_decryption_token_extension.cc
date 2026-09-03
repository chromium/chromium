// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/tab_context_decryption_token_extension.h"

#include <utility>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/uuid.h"
#include "chrome/common/tab_context_decryption_token_extension.mojom.h"
#include "components/sync/base/features.h"
#include "components/sync_tab_context/http_rpc_constants.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/array_buffer.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

// static
void TabContextDecryptionTokenExtension::Create(content::RenderFrame* frame) {
  new TabContextDecryptionTokenExtension(frame);
}

// static
bool TabContextDecryptionTokenExtension::
    ShouldExposeTabContextJavascriptApiForTesting(  // IN-TEST
        const url::Origin& origin,
        bool is_locked_to_site) {
  return ShouldExposeTabContextJavascriptApi(origin, is_locked_to_site);
}

// static
v8::Local<v8::Value>
TabContextDecryptionTokenExtension::CreateTokenValueForTesting(  // IN-TEST
    v8::Isolate* isolate,
    const std::optional<std::vector<uint8_t>>& token_bytes) {
  return CreateTokenValue(isolate, token_bytes);
}

// static
bool TabContextDecryptionTokenExtension::ShouldExposeTabContextJavascriptApi(
    const url::Origin& origin,
    bool is_locked_to_site) {
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEncryptedTabContextContainer)) {
    return false;
  }
  return origin == sync_tab_context::GetAllowedTabContextOrigin() &&
         is_locked_to_site;
}

// static
v8::Local<v8::Value> TabContextDecryptionTokenExtension::CreateTokenValue(
    v8::Isolate* isolate,
    const std::optional<std::vector<uint8_t>>& token_bytes) {
  if (!token_bytes.has_value()) {
    return v8::Null(isolate);
  }
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, token_bytes->size());
  gin::ArrayBuffer(array_buffer).span().copy_from(base::span(*token_bytes));
  return array_buffer;
}

TabContextDecryptionTokenExtension::TabContextDecryptionTokenExtension(
    content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

TabContextDecryptionTokenExtension::~TabContextDecryptionTokenExtension() =
    default;

void TabContextDecryptionTokenExtension::OnDestruct() {
  delete this;
}

void TabContextDecryptionTokenExtension::DidCreateScriptContext(
    v8::Local<v8::Context> v8_context,
    int32_t world_id) {
  if (!render_frame() || world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeTabContextJavascriptApi(
          render_frame()->GetWebFrame()->GetSecurityOrigin(),
          blink::Platform::Current()->IsLockedToSite())) {
    Install();
  }
}

void TabContextDecryptionTokenExtension::Install() {
  if (!render_frame()) {
    return;
  }

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);

  chrome
      ->Set(
          context, gin::StringToSymbol(isolate, "getContainerDecryptionToken"),
          gin::CreateFunctionTemplate(
              isolate, base::BindRepeating(&TabContextDecryptionTokenExtension::
                                               GetContainerDecryptionToken,
                                           weak_ptr_factory_.GetWeakPtr()))
              ->GetFunction(context)
              .ToLocalChecked())
      .Check();
}

void TabContextDecryptionTokenExtension::GetContainerDecryptionToken(
    v8::Local<v8::Function> callback,
    const std::string& obfuscated_gaia_id,
    const std::string& container_id_str) {
  if (!render_frame()) {
    return;
  }

  blink::WebLocalFrame* const web_frame = render_frame()->GetWebFrame();
  if (web_frame->MainWorldScriptContext().IsEmpty()) {
    return;
  }

  v8::Isolate* const isolate = web_frame->GetAgentGroupScheduler()->Isolate();

  const base::Uuid container_id =
      base::Uuid::ParseCaseInsensitive(container_id_str);
  if (!container_id.is_valid()) {
    DVLOG(1) << "Invalid container_id UUID format";
    isolate->ThrowException(v8::Exception::TypeError(gin::StringToV8(
        isolate, "Invalid container_id format; expected UUID.")));
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(isolate, callback);

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  remote_->GetContainerDecryptionToken(
      obfuscated_gaia_id, container_id,
      base::BindOnce(&TabContextDecryptionTokenExtension::
                         OnGetContainerDecryptionTokenResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(global_callback)));
}

void TabContextDecryptionTokenExtension::OnGetContainerDecryptionTokenResponse(
    std::unique_ptr<v8::Global<v8::Function>> callback,
    const std::optional<std::vector<uint8_t>>& token_bytes) {
  if (!render_frame()) {
    return;
  }

  blink::WebLocalFrame* const web_frame = render_frame()->GetWebFrame();
  v8::Isolate* const isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  const v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  const v8::Local<v8::Function> callback_local =
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

  v8::Local<v8::Value> token_value = CreateTokenValue(isolate, token_bytes);

  v8::Local<v8::Value> argv[] = {token_value};
  web_frame->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Undefined(isolate), /*argc=*/1, argv);
}
