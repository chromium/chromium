// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/printing_hooks_delegate.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace extensions {

namespace {

constexpr char kSubmitJobMethod[] = "printing.submitJob";

constexpr char kJobKey[] = "job";
constexpr char kDocumentKey[] = "document";

}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

PrintingHooksDelegate::PrintingHooksDelegate() = default;

PrintingHooksDelegate::~PrintingHooksDelegate() = default;

RequestResult PrintingHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  if (method_name != kSubmitJobMethod) {
    return RequestResult(RequestResult::NOT_HANDLED);
  }

  std::optional<RequestResult> parse_error =
      binding::ParseArgumentsForHook(*signature, context, arguments, refs);
  if (parse_error) {
    return std::move(*parse_error);
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Object> v8_submit_job_request =
      (*arguments)[0].As<v8::Object>();
  v8::Local<v8::Object> v8_print_job =
      v8_helpers::GetPropertyUnsafe(context, v8_submit_job_request, kJobKey)
          .As<v8::Object>();
  v8::Local<v8::Value> v8_document =
      v8_helpers::GetPropertyUnsafe(context, v8_print_job, kDocumentKey);

  base::expected<mojo::PendingRemote<blink::mojom::Blob>, RequestResult> blob =
      binding::TakeBlobArgument(v8_document);
  if (!blob.has_value()) {
    return std::move(blob.error());
  }

  RequestResult result(RequestResult::ARGUMENTS_UPDATED);
  result.blobs.push_back(std::move(*blob));
  CHECK(v8_print_job
            ->CreateDataProperty(
                context, v8_helpers::ToV8StringUnsafe(isolate, kDocumentKey),
                v8_document)
            .ToChecked());
  return result;
}

}  // namespace extensions
