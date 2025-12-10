// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_entry.h"

#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"

namespace contextual_search {

ContextualSearchSessionEntry::ContextualSearchSessionEntry(
    ContextualSearchSessionEntry&& other) noexcept
    : controller_(std::move(other.controller_)),
      metrics_recorder_(std::move(other.metrics_recorder_)),
      ref_count_(other.ref_count_) {
  if (controller_) {
    file_upload_status_observer_.Observe(controller_.get());
  }
}

ContextualSearchSessionEntry& ContextualSearchSessionEntry::operator=(
    ContextualSearchSessionEntry&& other) noexcept {
  if (this != &other) {
    controller_ = std::move(other.controller_);
    metrics_recorder_ = std::move(other.metrics_recorder_);
    ref_count_ = other.ref_count_;
    file_upload_status_observer_.Reset();
    if (controller_) {
      file_upload_status_observer_.Observe(controller_.get());
    }
  }
  return *this;
}

ContextualSearchSessionEntry::ContextualSearchSessionEntry(
    std::unique_ptr<ContextualSearchContextController> controller,
    std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder)
    : controller_(std::move(controller)),
      metrics_recorder_(std::move(metrics_recorder)) {
  if (controller_) {
    file_upload_status_observer_.Observe(controller_.get());
  }
}

ContextualSearchSessionEntry::~ContextualSearchSessionEntry() = default;

void ContextualSearchSessionEntry::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  if (metrics_recorder_) {
    metrics_recorder_->OnFileUploadStatusChanged(mime_type, file_upload_status,
                                                 error_type);
  }
}

}  // namespace contextual_search
