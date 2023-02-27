// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_relationship_verification/content_relationship_verification_constants.h"

#include "net/base/net_errors.h"

namespace content_relationship_verification {

const char kCustomCancelReasonForURLLoader[] =
    "ContentRelationshipVerification";

const int kNetErrorCodeForContentRelationshipVerification =
    net::ERR_ACCESS_DENIED;

}  // namespace content_relationship_verification
