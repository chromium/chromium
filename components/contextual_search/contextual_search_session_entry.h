// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_ENTRY_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_ENTRY_H_

#include <memory>

#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"

namespace contextual_search {
class ContextualSearchService;

// An entry in the session map, containing the ContextualSearchContextController
// and its reference count.
class ContextualSearchSessionEntry {
 public:
  ContextualSearchSessionEntry(const ContextualSearchSessionEntry&) = delete;
  ContextualSearchSessionEntry& operator=(const ContextualSearchSessionEntry&) =
      delete;
  ContextualSearchSessionEntry(ContextualSearchSessionEntry&&);
  ContextualSearchSessionEntry& operator=(ContextualSearchSessionEntry&&);
  ~ContextualSearchSessionEntry();

 private:
  friend class ContextualSearchService;
  explicit ContextualSearchSessionEntry(
      std::unique_ptr<ContextualSearchContextController> controller,
      std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder);

  std::unique_ptr<ContextualSearchContextController> controller_;
  std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder_;

  size_t ref_count_ = 1;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_ENTRY_H_
