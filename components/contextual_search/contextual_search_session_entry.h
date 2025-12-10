// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_ENTRY_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_ENTRY_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/lens/contextual_input.h"

namespace contextual_search {
class ContextualSearchService;

// An entry in the session map, containing the ContextualSearchContextController
// and its reference count.
class ContextualSearchSessionEntry
    : public ContextualSearchContextController::FileUploadStatusObserver {
 public:
  ContextualSearchSessionEntry(const ContextualSearchSessionEntry&) = delete;
  ContextualSearchSessionEntry& operator=(const ContextualSearchSessionEntry&) =
      delete;
  // Custom move operations are needed because base::ScopedObservation is not
  // movable.
  ContextualSearchSessionEntry(ContextualSearchSessionEntry&&) noexcept;
  ContextualSearchSessionEntry& operator=(
      ContextualSearchSessionEntry&&) noexcept;
  ~ContextualSearchSessionEntry() override;

 private:
  friend class ContextualSearchService;
  friend class ContextualSearchSessionEntryTest;
  explicit ContextualSearchSessionEntry(
      std::unique_ptr<ContextualSearchContextController> controller,
      std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder);

  // ContextualSearchContextController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

  std::unique_ptr<ContextualSearchContextController> controller_;
  std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder_;

  base::ScopedObservation<
      ContextualSearchContextController,
      ContextualSearchContextController::FileUploadStatusObserver>
      file_upload_status_observer_{this};

  size_t ref_count_ = 1;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_ENTRY_H_
