// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service_delegate.h"

namespace autofill {

LiveTabContextQuery::LiveTabContextQuery() = default;

LiveTabContextQuery::LiveTabContextQuery(const LiveTabContextQuery&) = default;

LiveTabContextQuery& LiveTabContextQuery::operator=(
    const LiveTabContextQuery&) = default;

LiveTabContextQuery::~LiveTabContextQuery() = default;

LiveTabContextResponse::LiveTabContextResponse() = default;

LiveTabContextResponse::LiveTabContextResponse(const LiveTabContextResponse&) =
    default;

LiveTabContextResponse& LiveTabContextResponse::operator=(
    const LiveTabContextResponse&) = default;

LiveTabContextResponse::~LiveTabContextResponse() = default;

}  // namespace autofill
