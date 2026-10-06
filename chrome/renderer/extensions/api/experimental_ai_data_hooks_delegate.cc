// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/experimental_ai_data_hooks_delegate.h"

#include <cstdint>
#include <utility>

#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace extensions {
namespace {

constexpr char kGetNodeForDomNodeIdMethod[] =
    "experimentalAiData.getNodeForDomNodeId";
constexpr char kGetDomNodeIdMethod[] = "experimentalAiData.getDomNodeId";
constexpr char kIncognitoNotSupportedError[] =
    "Incognito profile not supported.";

using RequestResult = APIBindingHooks::RequestResult;

RequestResult DenyAccess(v8::Isolate* isolate, const char* message) {
  isolate->ThrowException(
      v8::Exception::Error(gin::StringToV8(isolate, message)));
  return RequestResult(RequestResult::THROWN);
}

v8::Local<v8::Value> ConvertNode(v8::Isolate* isolate,
                                 ScriptContext* script_context,
                                 v8::Local<v8::Value> value,
                                 bool resolve_id) {
  // Feature definitions expose these helpers only in content-script contexts,
  // which always have a frame.
  blink::WebLocalFrame* frame = script_context->web_frame();

  blink::WebNode node;
  if (resolve_id) {
    // Do not coerce strings, truncate fractions, or wrap large numeric IDs.
    if (!value->IsInt32() || value.As<v8::Int32>()->Value() <= 0) {
      return v8::Null(isolate);
    }
    node = blink::WebNode::FromDomNodeId(value.As<v8::Int32>()->Value());
  } else {
    node = blink::WebNode::FromV8Value(isolate, value);
  }

  // IDs are renderer-wide, but this API is document-scoped. This check also
  // rejects nodes in other tabs, child frames, and documents before navigation.
  if (node.IsNull() || !node.IsConnected() ||
      node.GetDocument() != frame->GetDocument() ||
      node.IsInUserAgentShadowRoot() || node.IsPseudoElement()) {
    return v8::Null(isolate);
  }
  if (resolve_id) {
    // A connected node can still lack a JavaScript wrapper.
    v8::Local<v8::Value> wrapped_node = node.ToV8Value(isolate);
    if (wrapped_node.IsEmpty()) {
      return v8::Null(isolate);
    }
    return wrapped_node;
  }
  return v8::Integer::New(isolate, node.GetDomNodeId());
}

}  // namespace

RequestResult ExperimentalAiDataHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  const bool resolve_id = method_name == kGetNodeForDomNodeIdMethod;
  if (!resolve_id && method_name != kGetDomNodeIdMethod) {
    return RequestResult(RequestResult::NOT_HANDLED);
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  // Keep node access in regular profiles, just like APC snapshot capture.
  if (ExtensionsRendererClient::Get()->IsIncognitoProcess()) {
    return DenyAccess(isolate, kIncognitoNotSupportedError);
  }

  auto parsed = signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parsed.succeeded()) {
    return RequestResult(std::move(parsed.error.value()));
  }
  v8::Local<v8::Value> input = (*arguments)[0];
  RequestResult result(RequestResult::HANDLED);
  if (!input->IsArray()) {
    result.return_value =
        ConvertNode(isolate, script_context, input, resolve_id);
    return result;
  }

  // Keep the entire batch in C++ so hundreds of overlay anchors need only one
  // binding call. Preserve null entries rather than shifting later indices.
  v8::Local<v8::Array> array = input.As<v8::Array>();
  const uint32_t length = array->Length();
  v8::Local<v8::Array> output = v8::Array::New(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> value;
    if (!array->Get(context, i).ToLocal(&value)) {
      return RequestResult(RequestResult::THROWN);
    }
    // An array getter can run script and destroy its own frame.
    if (!script_context->is_valid()) {
      return RequestResult(RequestResult::CONTEXT_INVALIDATED);
    }
    if (!output
             ->CreateDataProperty(
                 context, i,
                 ConvertNode(isolate, script_context, value, resolve_id))
             .FromMaybe(false)) {
      return RequestResult(RequestResult::THROWN);
    }
  }
  result.return_value = output;
  return result;
}

}  // namespace extensions
