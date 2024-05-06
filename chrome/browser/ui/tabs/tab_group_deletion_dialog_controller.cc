// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/dialog_model.h"

namespace tab_groups {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDeletionDialogDontAskCheckboxId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDeletionDialogCancelButtonId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDeletionDialogOkButtonId);

namespace {

// TODO(b/331254038) replace these hardcoded strings with IDS strings.

// The text that shows on the checkbox.
constexpr char16_t kDontAsk[] = u"Don't ask again";

// For deletion, the text that shows on the dialog
constexpr char16_t kDeleteTitle[] = u"Delete tab group?";
constexpr char16_t kDeleteBody[] =
    u"Deleting the group will remove it from this device and other devices "
    u"using the same Google Account";
constexpr char16_t kDeleteOkText[] = u"Delete";

// For ungrouping, the text that shows on the dialog.
constexpr char16_t kUngroupTitle[] = u"Are you sure you want to ungroup?";
constexpr char16_t kUngroupBody[] =
    u"Ungrouping will leave the tabs open on this device but delete the group "
    u"on this device and other devices using the same Google Account";
constexpr char16_t kUngroupOkText[] = u"Ungroup";

// For closing the last tab, the text that shows on the dialog.
constexpr char16_t kCloseTabAndDeleteTitle[] = u"Close tab and delete group?";
constexpr char16_t kCloseTabAndDeleteBody[] =
    u"Closing the last tab will also delete the group from this device and "
    u"other devices using the same Google Account";
constexpr char16_t kCloseTabAndDeleteOkText[] = u"Close and delete group";

// For removing the last tab, the text that shows on the dialog.
constexpr char16_t kRemoveTabAndDeleteTitle[] = u"Remove tab and delete group?";
constexpr char16_t kRemoveTabAndDeleteBody[] =
    u"Removing the last tab will also delete the group from this device and "
    u"other devices using the same Google Account";
constexpr char16_t kRemoveTabAndDeleteOkText[] = u"Remove and delete group";

struct DialogText {
  const std::u16string title;
  const std::u16string body;
  const std::u16string ok_text;
};

// Returns the list of strings that are needed for a given dialog type.
DialogText GetDialogText(DeletionDialogController::DialogType type) {
  switch (type) {
    case DeletionDialogController::DialogType::DeleteSingle: {
      return DialogText{kDeleteTitle, kDeleteBody, kDeleteOkText};
    }
    case DeletionDialogController::DialogType::UngroupSingle: {
      return DialogText{kUngroupTitle, kUngroupBody, kUngroupOkText};
    }
    case DeletionDialogController::DialogType::RemoveTabAndDelete: {
      return DialogText{kRemoveTabAndDeleteTitle, kRemoveTabAndDeleteBody,
                        kRemoveTabAndDeleteOkText};
    }
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return DialogText{kCloseTabAndDeleteTitle, kCloseTabAndDeleteBody,
                        kCloseTabAndDeleteOkText};
    }
  }
}

// Returns the the value from the settings pref for a given dialog type.
bool IsDialogSkippedByUserSettings(Profile* profile,
                                   DeletionDialogController::DialogType type) {
  if (!profile) {
    return false;
  }
  PrefService* pref_service = profile->GetPrefs();
  switch (type) {
    case DeletionDialogController::DialogType::DeleteSingle: {
      return pref_service->GetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnDelete);
    }
    case DeletionDialogController::DialogType::UngroupSingle: {
      return pref_service->GetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnUngroup);
    }
    case DeletionDialogController::DialogType::RemoveTabAndDelete: {
      return pref_service->GetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnRemoveTab);
    }
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return pref_service->GetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnCloseTab);
    }
  }
}

void SetSkipDialogForType(Profile* profile,
                          DeletionDialogController::DialogType type,
                          bool new_value) {
  if (!profile) {
    return;
  }

  PrefService* pref_service = profile->GetPrefs();
  switch (type) {
    case DeletionDialogController::DialogType::DeleteSingle: {
      return pref_service->SetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnDelete, new_value);
    }
    case DeletionDialogController::DialogType::UngroupSingle: {
      return pref_service->SetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnUngroup, new_value);
    }
    case DeletionDialogController::DialogType::RemoveTabAndDelete: {
      return pref_service->SetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnRemoveTab, new_value);
    }
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return pref_service->SetBoolean(
          prefs::kTabGroupsDeletionSkipDialogOnCloseTab, new_value);
    }
  }
}

}  // anonymous namespace

DeletionDialogController::DialogState::DialogState(
    DialogType type_,
    ui::DialogModel* dialog_model_,
    base::OnceCallback<void()> on_ok_button_pressed_,
    base::OnceCallback<void()> on_cancel_button_pressed_)
    : type(type_),
      dialog_model(dialog_model_),
      on_ok_button_pressed(std::move(on_ok_button_pressed_)),
      on_cancel_button_pressed(std::move(on_cancel_button_pressed_)) {}

DeletionDialogController::DialogState::~DialogState() = default;

DeletionDialogController::DeletionDialogController(Browser* browser)
    : browser_(browser) {}

DeletionDialogController::~DeletionDialogController() = default;

bool DeletionDialogController::CanShowDialog() {
  return !IsShowingDialog();
}

bool DeletionDialogController::IsShowingDialog() {
  return state_ != nullptr;
}

bool DeletionDialogController::MaybeShowDialog(
    DialogType type,
    base::OnceCallback<void()> on_ok_callback) {
  if (!CanShowDialog()) {
    return false;
  }

  if (IsDialogSkippedByUserSettings(browser_->profile(), type)) {
    return false;
  }

  std::unique_ptr<ui::DialogModel> dialog_model = BuildDialogModel(type);

  state_ = std::make_unique<DeletionDialogController::DialogState>(
      type, dialog_model.get(), std::move(on_ok_callback), base::DoNothing());

  chrome::ShowBrowserModal(browser_, std::move(dialog_model));
  return true;
}

void DeletionDialogController::OnDialogOk() {
  if (state_->dialog_model &&
      state_->dialog_model
          ->GetCheckboxByUniqueId(kDeletionDialogDontAskCheckboxId)
          ->is_checked()) {
    SetSkipDialogForType(browser_->profile(), state_->type, true);
  }
  std::move(state_->on_ok_button_pressed).Run();
  state_.reset();
}

void DeletionDialogController::OnDialogCancel() {
  std::move(state_->on_cancel_button_pressed).Run();
  state_.reset();
}

std::unique_ptr<ui::DialogModel> DeletionDialogController::BuildDialogModel(
    DialogType type) {
  DialogText strings = GetDialogText(type);

  return ui::DialogModel::Builder()
      .SetTitle(strings.title)
      .AddParagraph(ui::DialogModelLabel(strings.body))
      .AddCheckbox(kDeletionDialogDontAskCheckboxId,
                   ui::DialogModelLabel(kDontAsk))
      .AddCancelButton(base::BindOnce(&DeletionDialogController::OnDialogCancel,
                                      base::Unretained(this)),
                       ui::DialogModel::Button::Params().SetEnabled(true).SetId(
                           kDeletionDialogCancelButtonId))
      .AddOkButton(base::BindOnce(&DeletionDialogController::OnDialogOk,
                                  base::Unretained(this)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(strings.ok_text)
                       .SetEnabled(true)
                       .SetId(kDeletionDialogOkButtonId))
      .Build();
}

}  // namespace tab_groups
