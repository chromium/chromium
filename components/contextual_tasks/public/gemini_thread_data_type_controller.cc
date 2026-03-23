// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/gemini_thread_data_type_controller.h"

#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"

namespace contextual_tasks {

GeminiThreadDataTypeController::GeminiThreadDataTypeController(
    ContextualTasksService* contextual_tasks_service,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode)
    : DataTypeController(syncer::GEMINI_THREAD,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      contextual_tasks_service_(contextual_tasks_service) {}

GeminiThreadDataTypeController::~GeminiThreadDataTypeController() = default;

syncer::DataTypeController::PreconditionState
GeminiThreadDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  // TODO(crbug.com/493203504): Add sync integration test.
  // TODO(crbug.com/493853682): Add in listener to update according to
  // Gemini thread eligibility.
  if (contextual_tasks_service_->IsGeminiThreadsEligible()) {
    return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
  }
  return syncer::DataTypeController::PreconditionState::kMustStopAndKeepData;
}

}  // namespace contextual_tasks
