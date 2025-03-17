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
    // Saved tab group dialogs.
    DeleteSingle,
    UngroupSingle,
    RemoveTabAndDelete,
    CloseTabAndDelete,
    // Shared tab group dialogs.
    DeleteSingleShared,
    CloseTabAndKeepOrLeaveGroup,
    CloseTabAndKeepOrDeleteGroup,
    LeaveGroup,
  };

  enum class DeletionDialogTiming {
    Synchronous,
    Asynchronous,
  };

  // Encapsulates metadata required to determine which strings should be
  // displayed in the deletion dialog.
  struct DialogMetadata {
    DialogMetadata(DialogType type,
                   int closing_group_count,
                   bool closing_multiple_tabs);
    // Used for testing.
    explicit DialogMetadata(DialogType type);
    DialogMetadata(const DialogMetadata&) = delete;

    ~DialogMetadata();

    DialogType type;
    int closing_group_count = 0;
    bool closing_multiple_tabs = false;
    std::optional<std::u16string> title_of_closing_group;
  };

  // State object that represents the current dialog that is being shown.
  struct DialogState {
    DialogState(DialogType type_,
                ui::DialogModel* dialog_model_,
                base::OnceCallback<void(DeletionDialogTiming)> callback_,
                std::optional<base::OnceClosure> keep_groups_);
    ~DialogState();

    // The type the dialog was initiated with.
    DialogType type;

    // A ptr to the original dialog model. Used to access the checkbox value
    // for setting the preference.
    raw_ptr<ui::DialogModel> dialog_model;

    // Callback that runs when the OK button is pressed.
    base::OnceCallback<void(DeletionDialogTiming)> callback;

    // Callback to handle the 'keep' case of CloseTabAndKeepOrLeaveGroup.
    std::optional<base::OnceClosure> keep_groups;
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
  // showing, and if the skip dialog option hasn't been set to true.
  // `dialog_metadata` contains information that is used to help construct the
  // strings for the dialog.
  bool MaybeShowDialog(
      const DialogMetadata& dialog_metadata,
      base::OnceCallback<void(DeletionDialogTiming)> callback,
      std::optional<base::OnceCallback<void()>> keep_groups = std::nullopt);

  void SetPrefsPreventShowingDialogForTesting(bool should_prevent_dialog);

 private:
  // Builds a DialogModel for showing the dialog.
  std::unique_ptr<ui::DialogModel> BuildDialogModel(
      const DialogMetadata& dialog_metadata);

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
