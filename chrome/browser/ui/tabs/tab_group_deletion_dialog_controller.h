// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_DELETION_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_DELETION_DIALOG_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"

namespace ui {
class DialogModel;
}

class Browser;
class Profile;

namespace tab_groups {

typedef base::RepeatingCallback<void(std::unique_ptr<ui::DialogModel>)>
    ShowDialogModelCallback;

// Controller that is responsible for showing/hiding and performing callbacks
// for group deletions. Manages the state on a per-browser basis. Browsers can
// only have 1 of these dialogs at a time, therefore only 1 controller. An
// example of this showing up is on Ungroup from the tab group editor bubble.
class DeletionDialogController {
 public:
  // Mapping of the different text strings and user preferences on this dialog.
  enum class DialogType {
    DeleteSingle,
    UngroupSingle,
    RemoveTabAndDelete,
    CloseTabAndDelete,
  };

  // State object that represents the current dialog that is being shown.
  struct DialogState {
    DialogState(DialogType type_,
                ui::DialogModel* dialog_model_,
                base::OnceClosure on_ok_button_pressed_,
                base::OnceClosure on_cancel_button_pressed_);
    ~DialogState();

    // The type the dialog was initiated with.
    DialogType type;

    // A ptr to the original dialog model. Used to access the checkbox value
    // for setting the preference.
    raw_ptr<ui::DialogModel> dialog_model;

    // Callback that runs when the OK button is pressed.
    base::OnceClosure on_ok_button_pressed;

    // Callback that runs when the Cancel button is pressed.
    base::OnceClosure on_cancel_button_pressed;
  };

  explicit DeletionDialogController(Browser* browser);
  DeletionDialogController(Profile* profile,
                           ShowDialogModelCallback show_dialog_model);

  DeletionDialogController(const DeletionDialogController&) = delete;
  DeletionDialogController& operator=(const DeletionDialogController&) = delete;
  ~DeletionDialogController();

  // If the BrowserWindow is currently in state where the dialog can be shown.
  bool CanShowDialog();

  // Whether the dialog is showing or not.
  bool IsShowingDialog();

  // Gets the dialog state for tests. Allows for calling the callbacks without
  // going through views code.
  void SimulateOkButtonForTesting() { OnDialogOk(); }

  // Attempt to show the dialog. The dialog will only show if it is not already
  // showing, and if the skip dialog option hasn't been set to true. tab_count
  // and group_count help construct strings for the dialog.
  bool MaybeShowDialog(DialogType type,
                       base::OnceCallback<void()> on_ok_callback,
                       int tab_count,
                       int group_count);

  void SetPrefsPreventShowingDialogForTesting(bool should_prevent_dialog);

 private:
  // Builds a DialogModel for showing the dialog.
  std::unique_ptr<ui::DialogModel> BuildDialogModel(DialogType type,
                                                    int tab_count = 0,
                                                    int group_count = 0);

  void CreateDialogFromBrowser(Browser* browser,
                               std::unique_ptr<ui::DialogModel> dialog_model);

  // Methods that are bound by the DialogModel to call the callbacks.
  void OnDialogOk();
  void OnDialogCancel();

  // The profile this controller is created for. Provides prefs and sync
  // settings.
  raw_ptr<Profile> profile_;

  // The state needed for showing the dialog. Only exists if the dialog is
  // currently showing.
  std::unique_ptr<DialogState> state_;

  // The function used to show the dialog when requested. This is injected so
  // that tests can instrument showing the dialog model.
  ShowDialogModelCallback show_dialog_model_fn_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_DELETION_DIALOG_CONTROLLER_H_
