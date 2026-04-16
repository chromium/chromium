// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_QUEUE_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"

namespace javascript_dialogs {

class AppModalDialogController;

// Keeps a queue of AppModalDialogControllers, making sure only one app modal
// dialog is shown at a time.
// This class is a singleton.
class AppModalDialogQueue {
 public:
  using DialogQueue =
      base::circular_deque<std::unique_ptr<AppModalDialogController>>;
  using iterator = DialogQueue::iterator;

  // Returns the singleton instance.
  static AppModalDialogQueue* GetInstance();

  AppModalDialogQueue(const AppModalDialogQueue&) = delete;
  AppModalDialogQueue& operator=(const AppModalDialogQueue&) = delete;

  // Adds a modal dialog to the queue. If there are no other dialogs in the
  // queue, the dialog will be shown immediately. Once it is shown, the
  // most recently active browser window (or whichever is currently active)
  // will be app modal, meaning it will be activated if the user tries to
  // activate any other browser windows.
  // Note: The AppModalDialogController `dialog` must be window modal before it
  // can be added as app modal.
  void AddDialog(std::unique_ptr<AppModalDialogController> dialog);

  // Removes the current dialog in the queue (the one that is being shown).
  // Shows the next dialog in the queue, if any is present. This does not
  // ensure that the currently showing dialog is closed, it just makes it no
  // longer app modal.
  void ShowNextDialog();

  // Cancels all pending dialogs and prevents new ones from being shown.
  // Must be called during browser shutdown to avoid showing dialogs after
  // profiles have been destroyed.
  void CancelAllDialogs();

  void ResetForTesting();

  // Activates and shows the current dialog, if the user clicks on one of the
  // windows disabled by the presence of an app modal dialog. This forces
  // the window to be visible on the display even if desktop manager software
  // opened the dialog on another virtual desktop. Assumes there is currently a
  // dialog being shown. (Call BrowserList::IsShowingAppModalDialog to test
  // this condition).
  void ActivateModalDialog();

  // Returns true if there is currently an active app modal dialog box.
  bool HasActiveDialog() const;

  AppModalDialogController* active_dialog() { return active_dialog_.get(); }

  // Iterators to walk the queue. The queue does not include the currently
  // active app modal dialog box.
  iterator begin() { return app_modal_dialog_queue_.begin(); }
  iterator end() { return app_modal_dialog_queue_.end(); }

 private:
  friend class base::NoDestructor<AppModalDialogQueue>;

  AppModalDialogQueue();
  ~AppModalDialogQueue();

  // Shows `dialog` and notifies the BrowserList that a modal dialog is showing.
  void ShowModalDialog(std::unique_ptr<AppModalDialogController> dialog);

  // Returns the next dialog to show. This removes entries from
  // app_modal_dialog_queue_ until one is valid or the queue is empty. This
  // returns nullptr if there are no more dialogs, or all the dialogs in the
  // queue are not valid.
  std::unique_ptr<AppModalDialogController> GetNextDialog();

  // Invalidates and clears all queued dialogs.
  void InvalidateAndClearQueuedDialogs();

  // Contains all app modal dialogs which are waiting to be shown. The currently
  // active modal dialog is not included.
  DialogQueue app_modal_dialog_queue_;

  // The currently active app-modal dialog box. `nullptr` if there is no active
  // app-modal dialog box.
  raw_ptr<AppModalDialogController> active_dialog_;

  // Stores if `ShowModalDialog()` is currently being called on an app-modal
  // dialog.
  bool showing_modal_dialog_;

  // Set to true when `CancelAllDialogs()` is called during browser shutdown.
  bool shutting_down_ = false;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_QUEUE_H_
