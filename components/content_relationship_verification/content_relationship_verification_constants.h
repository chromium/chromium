// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_CONTENT_RELATIONSHIP_VERIFICATION_CONSTANTS_H_
#define COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_CONTENT_RELATIONSHIP_VERIFICATION_CONSTANTS_H_
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

namespace content_relationship_verification {

// When a network::mojom::URLLoader is cancelled because of content relationship
// verification, this custom cancellation reason could be used to notify the
// implementation side. Please see
// network::mojom::URLLoader::kClientDisconnectReason for more details.
extern const char kCustomCancelReasonForURLLoader[];

// error_code to use when content relationship verification blocks a request.
extern const int kNetErrorCodeForContentRelationshipVerification;

// extended_reason() to use when content relationship verification blocks a
// request.
extern const blink::ResourceRequestBlockedReason kExtendedErrorReason;

}  // namespace content_relationship_verification

#endif  // COMPONENTS_CONTENT_RELATIONSHIP_VERIFICATION_CONTENT_RELATIONSHIP_VERIFICATION_CONSTANTS_H_
