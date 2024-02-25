// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"

namespace password_manager {

SingleLookupResponse::SingleLookupResponse() = default;

SingleLookupResponse::SingleLookupResponse(const SingleLookupResponse& other) =
    default;

SingleLookupResponse& SingleLookupResponse::operator=(
    const SingleLookupResponse& other) = default;

SingleLookupResponse::SingleLookupResponse(SingleLookupResponse&& other) =
    default;

SingleLookupResponse& SingleLookupResponse::operator=(
    SingleLookupResponse&& other) = default;

SingleLookupResponse::~SingleLookupResponse() = default;

}  // namespace password_manager
