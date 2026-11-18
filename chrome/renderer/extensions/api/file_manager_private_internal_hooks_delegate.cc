// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/file_manager_private_internal_hooks_delegate.h"

#include <optional>
#include <string>
#include <utility>

#include "extensions/renderer/bindings/api_binding_util.h"

namespace extensions {

using RequestResult = APIBindingHooks::RequestResult;

RequestResult FileManagerPrivateInternalHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  if (method_name != "fileManagerPrivateInternal.getContentMimeType" &&
      method_name != "fileManagerPrivateInternal.getContentMetadata") {
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

  RequestResult result(RequestResult::ARGUMENTS_UPDATED);
  result.blobs.push_back(std::move(*blob));
  return result;
}

}  // namespace extensions
