// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/google_accounts_private_api_extension.h"

#include "chrome/common/chrome_features.h"
#include "chrome/renderer/google_accounts_private_api_util.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"

// static
void GoogleAccountsPrivateApiExtension::Create(content::RenderFrame* frame) {
  new GoogleAccountsPrivateApiExtension(frame);
}

GoogleAccountsPrivateApiExtension::GoogleAccountsPrivateApiExtension(
    content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

GoogleAccountsPrivateApiExtension::~GoogleAccountsPrivateApiExtension() =
    default;

void GoogleAccountsPrivateApiExtension::OnDestruct() {
  delete this;
}

void GoogleAccountsPrivateApiExtension::DidCreateScriptContext(
    v8::Local<v8::Context> v8_context,
    int32_t world_id) {
  if (!render_frame() || world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeGoogleAccountsJavascriptApi(render_frame())) {
    InjectScript();
  }
}

void GoogleAccountsPrivateApiExtension::InjectScript() {
  DCHECK(render_frame());

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> window =
      content::GetOrCreateObject(isolate, context, "window");
  v8::Local<v8::Object> oAuthConsent =
      content::GetOrCreateObject(isolate, context, window, "OAuthConsent");

  oAuthConsent
      ->Set(
          context, gin::StringToSymbol(isolate, "setConsentResult"),
          gin::CreateFunctionTemplate(
              isolate, base::BindRepeating(
                           &GoogleAccountsPrivateApiExtension::SetConsentResult,
                           weak_ptr_factory_.GetWeakPtr()))
              ->GetFunction(context)
              .ToLocalChecked())
      .Check();
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
void GoogleAccountsPrivateApiExtension::SetConsentResult(gin::Arguments* args) {
  std::string consent_result;
  if (!args->GetNext(&consent_result)) {
    DLOG(ERROR) << "No consent result";
    args->ThrowError();
    return;
  }

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  remote_->SetConsentResult(consent_result);
}
#endif  // !BUILDFLAG(IS_ANDROID)
