// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/printing_hooks_delegate.h"

#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/web/web_blob.h"

namespace extensions {

namespace {

constexpr char kSubmitJobMethod[] = "printing.submitJob";

constexpr char kJobKey[] = "job";
constexpr char kDocumentKey[] = "document";
constexpr char kDocumentBlobUuidKey[] = "documentBlobUuid";

}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

PrintingHooksDelegate::PrintingHooksDelegate() = default;

PrintingHooksDelegate::~PrintingHooksDelegate() = default;

RequestResult PrintingHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  // Error checks.
  // Ensure we would like to call the SubmitJob function.
  if (method_name != kSubmitJobMethod)
    return RequestResult(RequestResult::NOT_HANDLED);
  // Ensure arguments are successfully parsed and converted.
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(parse_result.error.value());
    return result;
  }

  return HandleSubmitJob(context->GetIsolate(), arguments);
}

RequestResult PrintingHooksDelegate::HandleSubmitJob(
    v8::Isolate* isolate,
    std::vector<v8::Local<v8::Value>>* arguments) {
  DCHECK_EQ(2u, arguments->size());
  DCHECK((*arguments)[0]->IsObject());

  v8::Local<v8::Object> v8_submit_job_request =
      (*arguments)[0].As<v8::Object>();

  gin::Dictionary submit_job_request_dict(isolate, v8_submit_job_request);
  v8::Local<v8::Value> v8_print_job;
  if (!submit_job_request_dict.Get(kJobKey, &v8_print_job)) {
    NOTREACHED();
    return RequestResult(RequestResult::THROWN);
  }
  DCHECK(v8_print_job->IsObject());

  gin::Dictionary print_job_dict(isolate, v8_print_job.As<v8::Object>());
  v8::Local<v8::Value> v8_document;
  if (!print_job_dict.Get(kDocumentKey, &v8_document)) {
    NOTREACHED();
    return RequestResult(RequestResult::THROWN);
  }
  DCHECK(!v8_document.IsEmpty());
  DCHECK(!v8_document->IsNull());
  DCHECK(!v8_document->IsUndefined());

  blink::WebBlob document_blob = blink::WebBlob::FromV8Value(v8_document);
  v8::Local<v8::String> document_blob_uuid;
  v8_helpers::ToV8String(isolate, document_blob.Uuid().Utf8().data(),
                         &document_blob_uuid);
  if (!submit_job_request_dict.Set(kDocumentBlobUuidKey,
                                   document_blob_uuid.As<v8::Value>())) {
    NOTREACHED() << "Unexpected exception: couldn't update arguments";
    return RequestResult(RequestResult::THROWN);
  }

  return RequestResult(RequestResult::ARGUMENTS_UPDATED);
}

}  // namespace extensions
