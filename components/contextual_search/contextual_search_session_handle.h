// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"

namespace contextual_search {
using SessionId = base::UnguessableToken;
class ContextualSearchService;

// RAII handle for managing the lifetime of a ComposeboxQueryController.
class ContextualSearchSessionHandle {
 public:
  ContextualSearchSessionHandle(const ContextualSearchSessionHandle&) = delete;
  ContextualSearchSessionHandle& operator=(
      const ContextualSearchSessionHandle&) = delete;
  ContextualSearchSessionHandle(ContextualSearchSessionHandle&&) = delete;
  ContextualSearchSessionHandle& operator=(ContextualSearchSessionHandle&&) =
      delete;
  ~ContextualSearchSessionHandle();

  base::UnguessableToken session_id() const { return session_id_; }
  // Returns the ContextualSearchContextController reference held by this
  // handle or nullptr if the session is not valid.
  ContextualSearchContextController* GetController() const;

  // Returns the ContextualSearchMetricsRecorder reference held by this handle
  // or nullptr if the session is not valid.
  ContextualSearchMetricsRecorder* GetMetricsRecorder() const;

  std::string GetMetricsRecorderName() const;

 private:
  friend class ContextualSearchService;

  ContextualSearchSessionHandle(base::WeakPtr<ContextualSearchService> service,
                                const SessionId& session_id);

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSearchService> service_;
  const base::UnguessableToken session_id_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
