// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/public/collaboration_service.h"

namespace collaboration {

using ErrorInfo = CollaborationControllerDelegate::ErrorInfo;
using Outcome = CollaborationControllerDelegate::Outcome;
using ResultCallback = CollaborationControllerDelegate::ResultCallback;

// This is base class for each state and handles the logic for the state.
class ControllerState {
 public:
  ControllerState(CollaborationController::StateId id,
                  CollaborationController* controller)
      : id(id), controller(controller) {}
  virtual ~ControllerState() = default;

  // Called when entering the state.
  virtual void OnStart() {}

  // Called when an error happens during the state.
  virtual void HandleError() {
    controller->TransitionTo(std::make_unique<ControllerState>(
        CollaborationController::StateId::kError, controller));
  }

  // Called when the state finishes successfully.
  virtual void OnFinish() { controller->Exit(); }

  // Called to process the outcome of an external event.
  virtual void ProcessOutcome(Outcome outcome) {
    if (outcome == Outcome::kFailure) {
      HandleError();
      return;
    } else if (outcome == Outcome::kCancel) {
      controller->Exit();
      return;
    }

    OnFinish();
  }

  const CollaborationController::StateId id;
  const raw_ptr<CollaborationController> controller;
};

CollaborationController::CollaborationController(
    const Flow& flow,
    CollaborationService* collaboration_service,
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    FinishCallback finish_and_delete)
    : flow_(flow),
      collaboration_service_(collaboration_service),
      delegate_(std::move(delegate)),
      finish_and_delete_(std::move(finish_and_delete)) {}

CollaborationController::~CollaborationController() = default;

void CollaborationController::TransitionTo(
    std::unique_ptr<ControllerState> state) {
  DCHECK(IsValidStateTransition(current_state_->id, state->id));
  current_state_ = std::move(state);
  current_state_->OnStart();
}

void CollaborationController::Exit() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(finish_and_delete_)));
}

bool CollaborationController::IsValidStateTransition(StateId from, StateId to) {
  return std::find(kValidTransitions.begin(), kValidTransitions.end(),
                   std::make_pair(from, to)) != std::end(kValidTransitions);
}

}  // namespace collaboration
