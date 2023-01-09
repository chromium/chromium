// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/overlay_state/win/overlay_state_service.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"

namespace viz {

OverlayStateService::MailboxState::MailboxState() = default;
OverlayStateService::MailboxState::~MailboxState() = default;
OverlayStateService::OverlayStateService() = default;
OverlayStateService::~OverlayStateService() = default;

OverlayStateService* OverlayStateService::GetInstance() {
  // TODO(wicarr, crbug.com/1316009): Ideally the OverlayStateService should be
  // a singleton. Instead the GpuServiceImpl should be responsible for creating
  // the OverlayStateService and injecting it into dependent GpuChannel(s) and
  // the DCLayerOverlayProcessor. Further the OverlayStateService should live
  // in gpu to avoid gpu needing to take a dependency on viz.
  static base::NoDestructor<OverlayStateService> service_wrapper;
  return service_wrapper.get();
}

void OverlayStateService::Initialize(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // GpuServiceImpl is expected to initialize the OverlayStateService and
  // OverlayStateService will operate on the GpuMain sequenced task runner.
  // RegisterObserver is expected to be called by GpuChannel and should operate
  // on the same GpuMain sequence as GpuServiceImpl.
  // SetPromotionHint is called by DCLayerOverlayProcessor which operates on a
  // separate task sequence, VizCompositorThread, so calls are posted to
  // 'task_runner_' to allow mojo'ing back out to bound PromotionHintObserver
  // clients on the proper sequence.
  DCHECK(!initialized_);
  task_runner_ = std::move(task_runner);
  initialized_ = true;
}

bool OverlayStateService::IsInitialized() {
  return initialized_;
}

void OverlayStateService::RegisterObserver(
    mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
        overlay_state_observer,
    const gpu::Mailbox& mailbox) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  MailboxState*& mailbox_state = mailboxes_[mailbox];
  if (!mailbox_state) {
    mailbox_state = new MailboxState();
    // Use of base::Unretained is safe as OverlayStateService is a singleton
    // service bound to the lifetime of the GPU process.
    mailbox_state->observer_set_.set_disconnect_handler(
        base::BindRepeating(&OverlayStateService::OnBoundObserverDisconnect,
                            base::Unretained(this), mailbox));
  }

  // Add observer to the RemoteSet
  mojo::RemoteSetElementId id =
      mailbox_state->observer_set_.Add(std::move(overlay_state_observer));

  // It's possible that the overlay processor has already set promotion hint
  // information for this mailbox. If this is the case then we send a Hint
  // Changed event to the observer to inform them of the current promotion
  // state.
  OverlayStateAggregator::PromotionState promotion_state =
      mailbox_state->aggregator_.GetPromotionState();

  // If promotion state is unset there is no further work to do. If it is set
  // then inform the new observer of the current state.
  if (promotion_state != OverlayStateAggregator::PromotionState::kUnset) {
    bool promoted =
        promotion_state == OverlayStateAggregator::PromotionState::kPromoted;
    mailbox_state->observer_set_.Get(id)->OnStateChanged(promoted);
  }
}

void OverlayStateService::OnBoundObserverDisconnect(const gpu::Mailbox& mailbox,
                                                    mojo::RemoteSetElementId) {
  auto mailbox_iterator = mailboxes_.find(mailbox);
  if (mailbox_iterator != mailboxes_.end() &&
      mailbox_iterator->second->observer_set_.empty()) {
    // When the last observer has been disconnected, stop  tracking mailbox.
    mailboxes_.erase(mailbox_iterator);
  }
}

void OverlayStateService::OnStateChanged(
    const gpu::Mailbox& mailbox,
    OverlayStateAggregator::PromotionState promotion_state) {
  // If promotion state is unset there is no further work to do.
  if (promotion_state == OverlayStateAggregator::PromotionState::kUnset)
    return;

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Use of base::Unretained is safe as OverlayStateService is a singleton
    // service bound to the lifetime of the GPU process.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OverlayStateService::OnStateChangedOnTaskRunnerSequence,
                       base::Unretained(this), mailbox, promotion_state));
  } else {
    OnStateChangedOnTaskRunnerSequence(mailbox, promotion_state);
  }
}

void OverlayStateService::OnStateChangedOnTaskRunnerSequence(
    const gpu::Mailbox& mailbox,
    OverlayStateAggregator::PromotionState promotion_state) {
  // Notify all observers of the new hint state.
  bool promoted =
      promotion_state == OverlayStateAggregator::PromotionState::kPromoted;
  auto mailbox_iterator = mailboxes_.find(mailbox);
  DCHECK(mailbox_iterator != mailboxes_.end());
  for (auto& observer : mailbox_iterator->second->observer_set_) {
    observer->OnStateChanged(promoted);
  }
}

void OverlayStateService::SetPromotionHint(const gpu::Mailbox& mailbox,
                                           bool promoted) {
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());
  // Use of base::Unretained is safe as OverlayStateService is a singleton
  // service bound to the lifetime of the GPU process.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OverlayStateService::SetPromotionHintOnTaskRunnerSequence,
                     base::Unretained(this), mailbox, promoted));
}

void OverlayStateService::SetPromotionHintOnTaskRunnerSequence(
    const gpu::Mailbox& mailbox,
    bool promoted) {
  // The OverlayStateService is made aware of mailboxes of interest through two
  // channels:
  // 1.) The registration of an observer for a mailbox.
  // 2.) The setting of a promotion hint for a mailbox (the overlay processor
  //  will only send hints for mailboxes which are tagged as
  //  'wants_promotion_hint' on their TransferResource).
  //
  // If this is the first promotion hint associated with a mailbox which has
  // not had an observer registered yet, then we will not have an existing
  // entry in 'mailboxes_' tracking it. Since there is no guarantee when we'll
  // receive another promotion hint for the mailbox we'll store the result so
  // any future observer can be informed of the current promotion state.
  auto mailbox_iterator = mailboxes_.find(mailbox);
  if (mailbox_iterator != mailboxes_.end()) {
    bool state_change =
        mailbox_iterator->second->aggregator_.SetPromotionHint(promoted);
    if (state_change) {
      // Notifying observers requires an IPC so we only send an update when
      // the underlying state changes.
      OnStateChanged(mailbox,
                     mailbox_iterator->second->aggregator_.GetPromotionState());
    }
  } else {
    MailboxState* new_mailbox_state = new MailboxState();
    new_mailbox_state->aggregator_.SetPromotionHint(promoted);
    mailboxes_.insert({mailbox, new_mailbox_state});
  }
}

void OverlayStateService::MailboxDestroyed(const gpu::Mailbox& mailbox) {
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());
  // Use of base::Unretained is safe as OverlayStateService is a singleton
  // service bound to the lifetime of the GPU process.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OverlayStateService::MailboxDestroyedOnTaskRunnerSequence,
                     base::Unretained(this), mailbox));
}

void OverlayStateService::MailboxDestroyedOnTaskRunnerSequence(
    const gpu::Mailbox& mailbox) {
  mailboxes_.erase(mailbox);
}

}  // namespace viz
