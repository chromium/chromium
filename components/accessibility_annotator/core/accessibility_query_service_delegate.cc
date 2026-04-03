// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service_delegate.h"

namespace accessibility_annotator {

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

}  // namespace accessibility_annotator
