// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/media_galleries_hooks_delegate.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "extensions/renderer/v8_helpers.h"

namespace extensions {
namespace {

// On success, the bindings pass (callback, {metadata: ...}, optional Blob
// array). Merge the Blobs into metadata as metadata.attachedImages.
void GetMetadataCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> metadata = v8::Null(isolate);
  if (args.Length() > 1 && args[1]->IsObject() && !args[1]->IsArray()) {
    metadata = v8_helpers::GetPropertyUnsafe(context, args[1].As<v8::Object>(),
                                             "metadata");
    if (args.Length() > 2 && args[2]->IsArray()) {
      CHECK(metadata.As<v8::Object>()
                ->CreateDataProperty(
                    context,
                    v8_helpers::ToV8StringUnsafe(isolate, "attachedImages"),
                    args[2])
                .ToChecked());
    }
  }
  // The script callback receives just the metadata object, or null on error.
  if (args[0]->IsFunction()) {
    v8::Local<v8::Value> callback_args[] = {metadata};
    JSRunner::Get(context)->RunJSFunction(args[0].As<v8::Function>(), context,
                                          callback_args);
  }
}

}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

RequestResult MediaGalleriesHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  if (method_name != "mediaGalleries.getMetadata") {
    return RequestResult(RequestResult::NOT_HANDLED);
  }

  std::optional<RequestResult> parse_error =
      binding::ParseArgumentsForHook(*signature, context, arguments, refs);
  if (parse_error) {
    return std::move(*parse_error);
  }

  base::expected<mojo::PendingRemote<blink::mojom::Blob>, RequestResult> blob =
      binding::TakeBlobArgument((*arguments)[0]);
  if (!blob.has_value()) {
    return std::move(blob.error());
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  RequestResult result(
      RequestResult::ARGUMENTS_UPDATED,
      v8::Function::New(context, GetMetadataCallback).ToLocalChecked());
  result.blobs.push_back(std::move(*blob));
  // `options` must be non-null.
  if ((*arguments)[1]->IsNull()) {
    (*arguments)[1] = v8::Object::New(isolate);
  }
  return result;
}

}  // namespace extensions
