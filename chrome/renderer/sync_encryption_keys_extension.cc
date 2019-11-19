// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/sync_encryption_keys_extension.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

namespace {

const url::Origin& GetAllowedOrigin() {
  static const base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GaiaUrls::GetInstance()->gaia_url()));
  CHECK(!origin->opaque());
  return *origin;
}

}  // namespace

// static
void SyncEncryptionKeysExtension::Create(content::RenderFrame* frame) {
  new SyncEncryptionKeysExtension(frame);
}

SyncEncryptionKeysExtension::SyncEncryptionKeysExtension(
    content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

SyncEncryptionKeysExtension::~SyncEncryptionKeysExtension() {}

void SyncEncryptionKeysExtension::OnDestruct() {
  delete this;
}

void SyncEncryptionKeysExtension::DidCreateScriptContext(
    v8::Local<v8::Context> v8_context,
    int32_t world_id) {
  if (!render_frame()) {
    return;
  }

  url::Origin origin = render_frame()->GetWebFrame()->GetSecurityOrigin();
  if (render_frame()->IsMainFrame() &&
      world_id == content::ISOLATED_WORLD_ID_GLOBAL &&
      origin == GetAllowedOrigin()) {
    Install();
  }
}

void SyncEncryptionKeysExtension::Install() {
  DCHECK(render_frame());

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  v8::Local<v8::Function> function =
      gin::CreateFunctionTemplate(
          isolate, base::BindRepeating(
                       &SyncEncryptionKeysExtension::SetSyncEncryptionKeys,
                       weak_ptr_factory_.GetWeakPtr()))
          ->GetFunction(context)
          .ToLocalChecked();
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "setSyncEncryptionKeys"),
            function)
      .Check();
}

void SyncEncryptionKeysExtension::SetSyncEncryptionKeys(gin::Arguments* args) {
  DCHECK(render_frame());

  v8::HandleScope handle_scope(args->isolate());

  std::vector<std::string> encryption_keys;
  if (!args->GetNext(&encryption_keys)) {
    DLOG(ERROR) << "Not array of strings";
    args->ThrowError();
    return;
  }

  if (encryption_keys.empty()) {
    DLOG(ERROR) << "Array of strings empty";
    args->ThrowError();
    return;
  }

  std::string gaia_id;
  if (!args->GetNext(&gaia_id)) {
    DLOG(ERROR) << "No account ID";
    args->ThrowError();
    return;
  }

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    DLOG(ERROR) << "No callback";
    args->ThrowError();
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(args->isolate(), callback);

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  remote_->SetEncryptionKeys(
      encryption_keys, gaia_id,
      base::BindOnce(&SyncEncryptionKeysExtension::RunCompletionCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(global_callback)));
}

void SyncEncryptionKeysExtension::RunCompletionCallback(
    std::unique_ptr<v8::Global<v8::Function>> callback) {
  if (!render_frame()) {
    return;
  }

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> callback_local =
      v8::Local<v8::Function>::New(isolate, *callback);

  render_frame()->GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Undefined(isolate), /*argc=*/0, /*argv=*/{});
}
