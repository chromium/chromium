// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_queue.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"

namespace javascript_dialogs {

// static
AppModalDialogQueue* AppModalDialogQueue::GetInstance() {
  static base::NoDestructor<AppModalDialogQueue> instance;
  return instance.get();
}

void AppModalDialogQueue::CancelAllDialogs() {
  shutting_down_ = true;
  InvalidateAndClearQueuedDialogs();
}

void AppModalDialogQueue::ResetForTesting() {
  active_dialog_ = nullptr;
  shutting_down_ = false;
  InvalidateAndClearQueuedDialogs();
}

void AppModalDialogQueue::InvalidateAndClearQueuedDialogs() {
  // `AppModalDialogController::Invalidate()` runs the dialog's closed callback,
  // which can synchronously re-enter this queue: it can come back into this
  // method (e.g. via `ShowNextDialog()` during shutdown), and it can enqueue
  // new dialogs. Take ownership of the queued dialogs before invalidating any
  // of them, so that a re-entrant call sees an empty queue and cannot pop and
  // destroy a dialog whose `Invalidate()` is still on the stack.
  // See https://crbug.com/560439699.
  //
  // The outer loop handles dialogs that were enqueued by the re-entrant calls
  // made while invalidating; those dialogs must be invalidated as well.
  while (!app_modal_dialog_queue_.empty()) {
    DialogQueue dialogs = std::move(app_modal_dialog_queue_);
    // The state of a moved-from container is unspecified, so explicitly empty
    // the member queue for any re-entrant caller.
    app_modal_dialog_queue_.clear();
    for (std::unique_ptr<AppModalDialogController>& dialog : dialogs) {
      dialog->Invalidate();
    }
    // `dialogs` goes out of scope here, destroying the invalidated dialogs.
  }
}

void AppModalDialogQueue::AddDialog(
    std::unique_ptr<AppModalDialogController> dialog) {
  if (shutting_down_) {
    dialog->Invalidate();
    return;
  }

  if (!active_dialog_) {
    ShowModalDialog(std::move(dialog));
    return;
  }

  app_modal_dialog_queue_.push_back(std::move(dialog));
}

void AppModalDialogQueue::ShowNextDialog() {
  if (shutting_down_) {
    active_dialog_ = nullptr;
    InvalidateAndClearQueuedDialogs();
    return;
  }

  std::unique_ptr<AppModalDialogController> dialog = GetNextDialog();
  if (dialog) {
    ShowModalDialog(std::move(dialog));
  } else {
    active_dialog_ = nullptr;
  }
}

void AppModalDialogQueue::ActivateModalDialog() {
  if (showing_modal_dialog_) {
    // As part of showing a modal dialog we may end up back in this method
    // (showing a dialog activates the WebContents, which can trigger a call
    // to ActivateModalDialog). We ignore such a request as after the call to
    // activate the tab contents the dialog is shown.
    return;
  }

  if (active_dialog_) {
    active_dialog_->ActivateModalDialog();
  }
}

bool AppModalDialogQueue::HasActiveDialog() const {
  return !!active_dialog_;
}

AppModalDialogQueue::AppModalDialogQueue() = default;

AppModalDialogQueue::~AppModalDialogQueue() = default;

void AppModalDialogQueue::ShowModalDialog(
    std::unique_ptr<AppModalDialogController> dialog) {
  // Be sure and set the active_dialog_ field first, otherwise if
  // ShowModalDialog triggers a call back to the queue they'll get the old
  // dialog. Also, if the dialog calls `ShowNextDialog()` before returning, that
  // would write nullptr into `active_dialog_` and this function would then undo
  // that.
  active_dialog_ = dialog.get();
  showing_modal_dialog_ = true;
  active_dialog_->ShowModalDialog(std::move(dialog));
  showing_modal_dialog_ = false;
}

std::unique_ptr<AppModalDialogController> AppModalDialogQueue::GetNextDialog() {
  while (!app_modal_dialog_queue_.empty()) {
    std::unique_ptr<AppModalDialogController> dialog =
        std::move(app_modal_dialog_queue_.front());
    app_modal_dialog_queue_.pop_front();
    if (dialog->IsValid()) {
      return dialog;
    }
  }
  return nullptr;
}

}  // namespace javascript_dialogs
