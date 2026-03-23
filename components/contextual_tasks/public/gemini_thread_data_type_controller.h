// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_GEMINI_THREAD_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_GEMINI_THREAD_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/service/data_type_controller.h"

namespace contextual_tasks {

class ContextualTasksService;

class GeminiThreadDataTypeController : public syncer::DataTypeController {
 public:
  GeminiThreadDataTypeController(
      ContextualTasksService* contextual_tasks_service,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode);

  ~GeminiThreadDataTypeController() override;

  // DataTypeController implementation.
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

 private:
  raw_ptr<ContextualTasksService> contextual_tasks_service_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_GEMINI_THREAD_DATA_TYPE_CONTROLLER_H_
