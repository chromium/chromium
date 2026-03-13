// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/submitted_context_decorator.h"

#include "components/contextual_search/contextual_search_session_handle.h"

namespace contextual_tasks {

SubmittedContextDecorator::SubmittedContextDecorator() = default;
SubmittedContextDecorator::~SubmittedContextDecorator() = default;

std::vector<base::UnguessableToken>
SubmittedContextDecorator::GetTokensToDecorate(
    contextual_search::ContextualSearchSessionHandle* handle) const {
  return handle->GetSubmittedContextTokens();
}

}  // namespace contextual_tasks
