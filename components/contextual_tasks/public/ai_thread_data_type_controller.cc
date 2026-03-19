// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/ai_thread_data_type_controller.h"

#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"

namespace contextual_tasks {

AIThreadDataTypeController::AIThreadDataTypeController(
    AimEligibilityService* aim_eligibility_service,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode)
    : DataTypeController(syncer::AI_THREAD,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      aim_eligibility_service_(aim_eligibility_service) {}

AIThreadDataTypeController::~AIThreadDataTypeController() = default;

syncer::DataTypeController::PreconditionState
AIThreadDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  // TODO(crbug.com/493203504) add sync integration test.
  if (aim_eligibility_service_->IsAimEligible()) {
    return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
  }
  return syncer::DataTypeController::PreconditionState::kMustStopAndKeepData;
}

}  // namespace contextual_tasks
