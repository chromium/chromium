// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PRIVILEGED_CPP_BUCKET_CLIENT_INFO_H_
#define COMPONENTS_SERVICES_STORAGE_PRIVILEGED_CPP_BUCKET_CLIENT_INFO_H_

#include <cstdint>
#include <optional>

#include "third_party/blink/public/common/tokens/tokens.h"

namespace storage {

// Information about the browser process representation of an execution context
// (frame or worker) acting as the client of a bucket.
//
// Typemapping is maintained between this and storage.mojom.BucketClientInfo.
struct BucketClientInfo {
  // Comparators to enable use in STL containers.
  friend bool operator==(const BucketClientInfo& lhs,
                         const BucketClientInfo& rhs) = default;

  friend auto operator<=>(const BucketClientInfo& lhs,
                          const BucketClientInfo& rhs) {
    return lhs.context_token <=> rhs.context_token;
  }

  // The ID of the `RenderProcessHost` that the client belongs to.
  int32_t process_id;

  // A token that uniquely identifies the client's execution context.
  // Expected to only be a `LocalFrameToken`, `DedicatedWorkerToken`,
  // `ServiceWorkerToken`, or `SharedWorkerToken`.
  // This expectation is asserted in bucket_client_info_mojom_traits.cc.
  blink::ExecutionContextToken context_token;

  // The token of the document associated with the client, if there is one.
  // Expected to be set only if `context_token` is a `LocalFrameToken` or a
  // `DedicatedWorkerToken`.
  // This expectation is asserted in bucket_client_info_mojom_traits.cc.
  std::optional<blink::DocumentToken> document_token;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PRIVILEGED_CPP_BUCKET_CLIENT_INFO_H_
