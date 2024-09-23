// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/privileged/mojom/bucket_client_info_mojom_traits.h"

#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<storage::mojom::BucketClientInfoDataView,
                  storage::BucketClientInfo>::
    Read(storage::mojom::BucketClientInfoDataView data,
         storage::BucketClientInfo* out) {
  blink::ExecutionContextToken context_token;
  if (!data.ReadContextToken(&context_token)) {
    return false;
  }
  if (!context_token.Is<blink::LocalFrameToken>() &&
      !context_token.Is<blink::DedicatedWorkerToken>() &&
      !context_token.Is<blink::ServiceWorkerToken>() &&
      !context_token.Is<blink::SharedWorkerToken>()) {
    return false;
  }
  std::optional<blink::DocumentToken> document_token;
  if (!data.ReadDocumentToken(&document_token)) {
    return false;
  }
  if (document_token.has_value() &&
      !context_token.Is<blink::LocalFrameToken>() &&
      !context_token.Is<blink::DedicatedWorkerToken>()) {
    return false;
  }

  *out = storage::BucketClientInfo{data.process_id(), context_token,
                                   document_token};
  return true;
}

}  // namespace mojo
