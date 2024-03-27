// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
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

}  // anonymous namespace

DeletionDialogController::DialogState::DialogState(
    base::OnceCallback<void()> on_ok_button_pressed_,
    base::OnceCallback<void()> on_cancel_button_pressed_)
    : on_ok_button_pressed(std::move(on_ok_button_pressed_)),
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

// Attempt to show the dialog. The dialog will only show if it is not already
// showing.
bool DeletionDialogController::MaybeShowDialog(
    DialogType type,
    base::OnceCallback<void()> on_ok_callback) {
  if (!CanShowDialog()) {
    return false;
  }
  state_ = std::make_unique<DeletionDialogController::DialogState>(
      std::move(on_ok_callback), base::DoNothing());

  chrome::ShowBrowserModal(browser_, BuildDialogModel(type));
  return true;
}

void DeletionDialogController::OnDialogOk() {
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
