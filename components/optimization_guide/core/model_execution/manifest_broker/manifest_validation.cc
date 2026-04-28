// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_validation.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

ManifestValidator::ManifestValidator(
    OnDeviceModelAccessController& access_controller,
    ModelBrokerImpl& model_broker)
    : access_controller_(access_controller), model_broker_(model_broker) {}

ManifestValidator::~ManifestValidator() {
  TRACE_EVENT("optimization_guide", "ManifestValidator::~ManifestValidator",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ManifestValidator::MaybeExecuteValidationTask(
    const proto::ValidationTask& task) {
  TRACE_EVENT("optimization_guide",
              "ManifestValidator::MaybeExecuteValidationTask", "version",
              task.version(), perfetto::Flow::FromPointer(this));
  // Check if there is a new validation to do, but don't start an attempt yet,
  // so that we don't exhaust attempts before we actually have the needed
  // assets.
  if (!access_controller_->HasRemainingValidationAttempts(task.version())) {
    return;
  }
  // Cancel any prior validation
  weak_ptr_factory_.InvalidateWeakPtrs();
  validator_.reset();
  task_ = task;
  model_broker_->GetSolutionProvider(task_.use_case())
      .local_subscriber()
      .WaitForClient(base::BindOnce(&ManifestValidator::OnClientAvailable,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ManifestValidator::OnClientAvailable(base::WeakPtr<ModelClient> client) {
  TRACE_EVENT("optimization_guide", "ManifestValidator::OnClientAvailable",
              "version", task_.version(), perfetto::Flow::FromPointer(this));
  // Don't start immediately when assets are first available, to let other tasks
  // that actually requested the assets to start first.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ManifestValidator::StartValidation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client)),
      features::GetOnDeviceModelValidationDelay());
}

void ManifestValidator::StartValidation(base::WeakPtr<ModelClient> client) {
  TRACE_EVENT("optimization_guide", "ManifestValidator::StartValidation",
              "version", task_.version(), perfetto::Flow::FromPointer(this));
  if (!client || !access_controller_->MaybeBeginValidation(task_.version())) {
    // Client doesn't support the validation. This is unlikely.
    return;
  }

  mojo::Remote<on_device_model::mojom::Session> session;
  client->solution().CreateSession(
      session.BindNewPipeAndPassReceiver(),
      on_device_model::mojom::SessionParams::New());
  validator_ = std::make_unique<OnDeviceModelValidator>(
      task_.config(),
      base::BindOnce(&ManifestValidator::OnValidationComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(session));
}

void ManifestValidator::OnValidationComplete(
    OnDeviceModelValidationResult result) {
  TRACE_EVENT("optimization_guide", "ManifestValidator::OnValidationComplete",
              "version", task_.version(), "result", result,
              perfetto::Flow::FromPointer(this));
  access_controller_->OnValidationFinished(result);
  validator_.reset();
}

}  // namespace optimization_guide
