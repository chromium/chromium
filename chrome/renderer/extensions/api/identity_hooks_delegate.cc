// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/identity_hooks_delegate.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/v8_helpers.h"

namespace extensions {

namespace {
constexpr char kGetAuthToken[] = "identity.getAuthToken";

// Function that, for callback based API calls, will make the values associated
// with each property on the return object a separate argument in a new result
// vector it returns instead.
// Note: This is to allow the promise version of the API to return a
// single object, while still supporting the previous callback version which
// expects multiple parameters to be passed to the callback.
v8::LocalVector<v8::Value> MassageGetAuthTokenResults(
    const v8::LocalVector<v8::Value>& result_args,
    v8::Local<v8::Context> context,
    binding::AsyncResponseType async_type) {
  // If this is not a callback based API call, we don't need to modify anything.
  if (async_type != binding::AsyncResponseType::kCallback) {
    return result_args;
  }

  DCHECK_EQ(1u, result_args.size());
  DCHECK(result_args[0]->IsObject());
  v8::Local<v8::Object> result_obj = result_args[0].As<v8::Object>();

  // The object sent back has two properties on it which we need to split into
  // two separate arguments"
  v8::Local<v8::Value> token;
  bool success = v8_helpers::GetProperty(context, result_obj, "token", &token);
  DCHECK(success);
  v8::Local<v8::Value> granted_scopes;
  success = v8_helpers::GetProperty(context, result_obj, "grantedScopes",
                                    &granted_scopes);
  DCHECK(success);
  v8::LocalVector<v8::Value> new_args(context->GetIsolate(),
                                      {token, granted_scopes});

  return new_args;
}

}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

IdentityHooksDelegate::IdentityHooksDelegate() = default;
IdentityHooksDelegate::~IdentityHooksDelegate() = default;

RequestResult IdentityHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  // Only add the result handler for the getAuthToken function.
  if (method_name != kGetAuthToken)
    return RequestResult(RequestResult::NOT_HANDLED);

  return RequestResult(RequestResult::NOT_HANDLED,
                       v8::Local<v8::Function>() /*custom_callback*/,
                       base::BindOnce(MassageGetAuthTokenResults));
}

}  // namespace extensions
