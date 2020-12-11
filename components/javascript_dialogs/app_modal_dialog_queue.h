// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_QUEUE_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_QUEUE_H_

#include "base/containers/circular_deque.h"
#include "base/macros.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace javascript_dialogs {

class AppModalDialogController;

// Keeps a queue of AppModalDialogControllers, making sure only one app modal
// dialog is shown at a time.
// This class is a singleton.
class AppModalDialogQueue {
 public:
  typedef base::circular_deque<AppModalDialogController*>::iterator iterator;

  // Returns the singleton instance.
  static AppModalDialogQueue* GetInstance();

  // Adds a modal dialog to the queue. If there are no other dialogs in the
  // queue, the dialog will be shown immediately. Once it is shown, the
  // most recently active browser window (or whichever is currently active)
  // will be app modal, meaning it will be activated if the user tries to
  // activate any other browser windows.
  // Note: The AppModalDialogController |dialog| must be window modal before it
  // can be added as app modal.
  void AddDialog(AppModalDialogController* dialog);

  // Removes the current dialog in the queue (the one that is being shown).
  // Shows the next dialog in the queue, if any is present. This does not
  // ensure that the currently showing dialog is closed, it just makes it no
  // longer app modal.
  void ShowNextDialog();

  // Activates and shows the current dialog, if the user clicks on one of the
  // windows disabled by the presence of an app modal dialog. This forces
  // the window to be visible on the display even if desktop manager software
  // opened the dialog on another virtual desktop. Assumes there is currently a
  // dialog being shown. (Call BrowserList::IsShowingAppModalDialog to test
  // this condition).
  void ActivateModalDialog();

  // Returns true if there is currently an active app modal dialog box.
  bool HasActiveDialog() const;

  AppModalDialogController* active_dialog() { return active_dialog_; }

  // Iterators to walk the queue. The queue does not include the currently
  // active app modal dialog box.
  iterator begin() { return app_modal_dialog_queue_.begin(); }
  iterator end() { return app_modal_dialog_queue_.end(); }

 private:
  friend struct base::DefaultSingletonTraits<AppModalDialogQueue>;

  AppModalDialogQueue();
  ~AppModalDialogQueue();

  // Shows |dialog| and notifies the BrowserList that a modal dialog is showing.
  void ShowModalDialog(AppModalDialogController* dialog);

  // Returns the next dialog to show. This removes entries from
  // app_modal_dialog_queue_ until one is valid or the queue is empty. This
  // returns nullptr if there are no more dialogs, or all the dialogs in the
  // queue are not valid.
  AppModalDialogController* GetNextDialog();

  // Contains all app modal dialogs which are waiting to be shown. The currently
  // active modal dialog is not included.
  base::circular_deque<AppModalDialogController*> app_modal_dialog_queue_;

  // The currently active app-modal dialog box. nullptr if there is no active
  // app-modal dialog box.
  AppModalDialogController* active_dialog_;

  // Stores if |ShowModalDialog()| is currently being called on an app-modal
  // dialog.
  bool showing_modal_dialog_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialogQueue);
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_QUEUE_H_
