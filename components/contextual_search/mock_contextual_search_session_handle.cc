// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/mock_contextual_search_session_handle.h"

#include "components/contextual_search/contextual_search_session_handle.h"

namespace contextual_search {

MockContextualSearchSessionHandle::MockContextualSearchSessionHandle()
    : ContextualSearchSessionHandle(nullptr, base::UnguessableToken::Create()) {
}
MockContextualSearchSessionHandle::~MockContextualSearchSessionHandle() =
    default;

}  // namespace contextual_search
