// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/sync_encryption_keys_extension.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "chrome/renderer/google_accounts_private_api_util.h"
#include "components/sync/base/features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace {

// This function is intended to convert a binary blob representing an encryption
// key and provided by the web via a Javascript ArrayBuffer.
std::vector<uint8_t> ArrayBufferAsBytes(
    const v8::Local<v8::ArrayBuffer>& array_buffer) {
  auto backing_store = array_buffer->GetBackingStore();
  const uint8_t* start =
      reinterpret_cast<const uint8_t*>(backing_store->Data());
  const size_t length = backing_store->ByteLength();
  return std::vector<uint8_t>(start, start + length);
}

std::vector<std::vector<uint8_t>> EncryptionKeysAsBytes(
    const std::vector<v8::Local<v8::ArrayBuffer>>& encryption_keys) {
  std::vector<std::vector<uint8_t>> encryption_keys_as_bytes;
  for (const v8::Local<v8::ArrayBuffer>& encryption_key : encryption_keys) {
    encryption_keys_as_bytes.push_back(ArrayBufferAsBytes(encryption_key));
  }
  return encryption_keys_as_bytes;
}

void RecordCallToSetSyncEncryptionKeysToUma(bool valid_args) {
  base::UmaHistogramBoolean(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysValidArgs", valid_args);
}

void RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(bool valid_args) {
  base::UmaHistogramBoolean(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodValidArgs", valid_args);
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
  if (!render_frame() || world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeGoogleAccountsJavascriptApi(render_frame())) {
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

  // On Android, there is no existing plumbing for setSyncEncryptionKeys(), so
  // let's not expose the Javascript function as available. Namely,
  // TrustedVaultClientAndroid::StoreKeys() isn't implemented because there is
  // no underlying Android API to invoke, given that sign in and reauth flows
  // are handled outside the browser.
#if !BUILDFLAG(IS_ANDROID)
  chrome
      ->Set(
          context, gin::StringToSymbol(isolate, "setSyncEncryptionKeys"),
          gin::CreateFunctionTemplate(
              isolate, base::BindRepeating(
                           &SyncEncryptionKeysExtension::SetSyncEncryptionKeys,
                           weak_ptr_factory_.GetWeakPtr()))
              ->GetFunction(context)
              .ToLocalChecked())
      .Check();
#endif

  chrome
      ->Set(context,
            gin::StringToSymbol(isolate,
                                "addTrustedSyncEncryptionRecoveryMethod"),
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(&SyncEncryptionKeysExtension::
                                        AddTrustedSyncEncryptionRecoveryMethod,
                                    weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
}

void SyncEncryptionKeysExtension::SetSyncEncryptionKeys(gin::Arguments* args) {
  DCHECK(render_frame());

  // This function as exposed to the web has the following signature:
  //   setSyncEncryptionKeys(callback, gaia_id, encryption_keys,
  //                         last_key_version)
  //
  // Where:
  //   callback: Allows caller to get notified upon completion.
  //   gaia_id: String representing the user's server-provided ID.
  //   encryption_keys: Array where each element is an ArrayBuffer representing
  //                    an encryption key (binary blob).
  //   last_key_version: Key version corresponding to the last key in
  //                     |encryption_keys|.

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No callback";
    args->ThrowError();
    return;
  }

  std::string gaia_id;
  if (!args->GetNext(&gaia_id)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No account ID";
    args->ThrowError();
    return;
  }

  std::vector<v8::Local<v8::ArrayBuffer>> encryption_keys;
  if (!args->GetNext(&encryption_keys)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "Not array of strings";
    args->ThrowError();
    return;
  }

  if (encryption_keys.empty()) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "Array of strings empty";
    args->ThrowError();
    return;
  }

  int last_key_version = 0;
  if (!args->GetNext(&last_key_version)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No version provided";
    args->ThrowError();
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(args->isolate(), callback);

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  RecordCallToSetSyncEncryptionKeysToUma(/*valid_args=*/true);
  remote_->SetEncryptionKeys(
      gaia_id, EncryptionKeysAsBytes(encryption_keys), last_key_version,
      base::BindOnce(&SyncEncryptionKeysExtension::RunCompletionCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(global_callback)));
}

void SyncEncryptionKeysExtension::AddTrustedSyncEncryptionRecoveryMethod(
    gin::Arguments* args) {
  DCHECK(render_frame());

  // This function as exposed to the web has the following signature:
  //   addTrustedSyncEncryptionRecoveryMethod(callback, gaia_id, public_key,
  //                                          method_type_hint)
  //
  // Where:
  //   callback: Allows caller to get notified upon completion.
  //   gaia_id: String representing the user's server-provided ID.
  //   public_key: A public key representing the recovery method to be added.
  //   method_type_hint: An enum-like integer representing the added method's
  //   type. This value is opaque to the client and may only be used for
  //   future related interactions with the server.

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No callback";
    args->ThrowError();
    return;
  }

  std::string gaia_id;
  if (!args->GetNext(&gaia_id)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No account ID";
    args->ThrowError();
    return;
  }

  v8::Local<v8::ArrayBuffer> public_key;
  if (!args->GetNext(&public_key)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No public key";
    args->ThrowError();
    return;
  }

  int method_type_hint = 0;
  if (!args->GetNext(&method_type_hint)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No method type hint";
    args->ThrowError();
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(args->isolate(), callback);

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(/*valid_args=*/true);
  remote_->AddTrustedRecoveryMethod(
      gaia_id, ArrayBufferAsBytes(public_key), method_type_hint,
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
