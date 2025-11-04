// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"

namespace contextual_search {

ContextualSearchSessionHandle::ContextualSearchSessionHandle(
    base::WeakPtr<ContextualSearchService> service,
    const base::UnguessableToken& session_id)
    : service_(service), session_id_(session_id) {}

ContextualSearchSessionHandle::~ContextualSearchSessionHandle() {
  if (service_) {
    service_->ReleaseSession(session_id_);
  }
}

ContextualSearchContextController*
ContextualSearchSessionHandle::GetController() const {
  return service_ ? service_->GetSessionController(session_id_) : nullptr;
}

ContextualSearchMetricsRecorder*
ContextualSearchSessionHandle::GetMetricsRecorder() const {
  return service_ ? service_->GetSessionMetricsRecorder(session_id_) : nullptr;
}

std::string ContextualSearchSessionHandle::GetMetricsRecorderName() const {
  return service_ ? service_->GetSessionMetricsRecorderName(session_id_) : "";
}

}  // namespace contextual_search
