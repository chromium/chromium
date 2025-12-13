// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXT_DECORATION_PARAMS_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXT_DECORATION_PARAMS_H_

#include "base/memory/weak_ptr.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {

// Parameters passed to all context decorators to guide the decoration
// process.
struct ContextDecorationParams {
  ContextDecorationParams();
  ~ContextDecorationParams();
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      contextual_search_session_handle;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXT_DECORATION_PARAMS_H_
