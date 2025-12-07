// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"

#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_pref_names.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace tab_groups {

namespace {
// The text that shows on the checkbox.
constexpr int kDontAskId = IDS_TAB_GROUP_DELETION_DIALOG_DONT_ASK;

// Body text for all delete actions.
constexpr int kDeleteBodySyncedId =
    IDS_TAB_GROUP_DELETION_DIALOG_BODY_SYNCED_DELETE;
constexpr int kDeleteBodyNotSyncedId =
    IDS_TAB_GROUP_DELETION_DIALOG_BODY_NOT_SYNCED_DELETE;

// For deletion, the text that shows on the dialog.
constexpr int kDeleteTitleId = IDS_TAB_GROUP_DELETION_DIALOG_TITLE_DELETE;
constexpr int kDeleteOkTextId = IDS_TAB_GROUP_DELETION_DIALOG_OK_TEXT_DELETE;

// For ungrouping, the text that shows on the dialog.
constexpr int kUngroupTitleId = IDS_TAB_GROUP_DELETION_DIALOG_TITLE_UNGROUP;
constexpr int kUngroupBodySyncedId =
    IDS_TAB_GROUP_DELETION_DIALOG_BODY_SYNCED_UNGROUP;
constexpr int kUngroupBodyNotSyncedId =
    IDS_TAB_GROUP_DELETION_DIALOG_BODY_NOT_SYNCED_UNGROUP;
constexpr int kUngroupOkTextId = IDS_TAB_GROUP_DELETION_DIALOG_OK_TEXT_UNGROUP;

// For closing the last tab, the text that shows on the dialog.
constexpr int kCloseTabAndDeleteTitleId =
    IDS_TAB_GROUP_DELETION_DIALOG_TITLE_CLOSE_TAB_AND_DELETE;

// For removing the last tab, the text that shows on the dialog.
constexpr int kRemoveTabAndDeleteTitleId =
    IDS_TAB_GROUP_DELETION_DIALOG_TITLE_REMOVE_TAB_AND_DELETE;

struct DialogText {
  const std::u16string title;
  const std::u16string body;
  const std::u16string ok_text;
  const std::optional<std::u16string> cancel_text = std::nullopt;
};

// Returns the list of strings that are needed for a given dialog type.
DialogText GetDialogText(
    Profile* profile,
    const DeletionDialogController::DialogMetadata& dialog_metadata) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);

  const bool is_sync_enabled =
      tab_group_service &&
      tab_groups::SavedTabGroupUtils::AreSavedTabGroupsSyncedForProfile(
          profile);

  std::u16string email =
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_DELETION_DIALOG_MISSING_EMAIL);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (profile && identity_manager) {
    email = base::UTF8ToUTF16(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email);
  }

  const int closing_group_count = dialog_metadata.closing_group_count;
  const int closing_multiple_groups = closing_group_count > 1;
  const int plural_type_count =
      dialog_metadata.closing_multiple_tabs + closing_multiple_groups;

  switch (dialog_metadata.type) {
    case DeletionDialogController::DialogType::DeleteSingle: {
      return DialogText{
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kDeleteTitleId), closing_group_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              is_sync_enabled
                  ? l10n_util::GetStringFUTF16(kDeleteBodySyncedId, email)
                  : l10n_util::GetStringUTF16(kDeleteBodyNotSyncedId),
              closing_group_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kDeleteOkTextId), closing_group_count)};
    }
    case DeletionDialogController::DialogType::UngroupSingle: {
      return DialogText{
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kUngroupTitleId), closing_group_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              is_sync_enabled
                  ? l10n_util::GetStringFUTF16(kUngroupBodySyncedId, email)
                  : l10n_util::GetStringUTF16(kUngroupBodyNotSyncedId),
              closing_group_count),
          l10n_util::GetStringUTF16(kUngroupOkTextId)};
    }
    case DeletionDialogController::DialogType::RemoveTabAndDelete: {
      return DialogText{
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kRemoveTabAndDeleteTitleId),
              plural_type_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              is_sync_enabled
                  ? l10n_util::GetStringFUTF16(kDeleteBodySyncedId, email)
                  : l10n_util::GetStringUTF16(kDeleteBodyNotSyncedId),
              closing_group_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kDeleteOkTextId), closing_group_count)};
    }
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return DialogText{
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kCloseTabAndDeleteTitleId),
              plural_type_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              is_sync_enabled
                  ? l10n_util::GetStringFUTF16(kDeleteBodySyncedId, email)
                  : l10n_util::GetStringUTF16(kDeleteBodyNotSyncedId),
              closing_group_count),
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(kDeleteOkTextId), closing_group_count)};
    }
    case DeletionDialogController::DialogType::DeleteSingleShared: {
      const bool title_is_empty =
          !dialog_metadata.title_of_closing_group.has_value() ||
          dialog_metadata.title_of_closing_group->empty();
      std::u16string body_text =
          title_is_empty
              ? l10n_util::GetStringUTF16(
                    IDS_DATA_SHARING_OWNER_DELETE_DIALOG_BODY_NO_GROUP_TITLE)
              : l10n_util::GetStringFUTF16(
                    IDS_DATA_SHARING_OWNER_DELETE_DIALOG_BODY,
                    dialog_metadata.title_of_closing_group.value());
      return DialogText{
          l10n_util::GetStringUTF16(IDS_DATA_SHARING_OWNER_DELETE_DIALOG_TITLE),
          std::move(body_text),
          l10n_util::GetStringUTF16(
              IDS_DATA_SHARING_OWNER_DELETE_DIALOG_CONFIRM)};
    }
    case DeletionDialogController::DialogType::CloseTabAndKeepOrLeaveGroup: {
      return DialogText{
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_DELETE_LAST_TAB_TITLE,
              dialog_metadata.closing_group_count),
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_MEMBER_DELETE_LAST_TAB_BODY,
              dialog_metadata.closing_group_count),
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_DELETE_LAST_TAB_CONFIRM,
              dialog_metadata.closing_group_count),
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_MEMBER_DELETE_LAST_TAB_CANCEL,
              dialog_metadata.closing_group_count),
      };
    }
    case DeletionDialogController::DialogType::CloseTabAndKeepOrDeleteGroup: {
      return DialogText{
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_DELETE_LAST_TAB_TITLE,
              dialog_metadata.closing_group_count),
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_OWNER_DELETE_LAST_TAB_BODY,
              dialog_metadata.closing_group_count),
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_DELETE_LAST_TAB_CONFIRM,
              dialog_metadata.closing_group_count),
          l10n_util::GetPluralStringFUTF16(
              IDS_DATA_SHARING_OWNER_DELETE_LAST_TAB_CANCEL,
              dialog_metadata.closing_group_count),
      };
    }
    case DeletionDialogController::DialogType::LeaveGroup: {
      const bool title_is_empty =
          !dialog_metadata.title_of_closing_group.has_value() ||
          dialog_metadata.title_of_closing_group->empty();
      std::u16string body_text =
          title_is_empty
              ? l10n_util::GetStringUTF16(
                    IDS_DATA_SHARING_LEAVE_DIALOG_BODY_NO_GROUP_TITLE)
              : l10n_util::GetStringFUTF16(
                    IDS_DATA_SHARING_LEAVE_DIALOG_BODY,
                    dialog_metadata.title_of_closing_group.value());

      return DialogText{
          l10n_util::GetStringUTF16(IDS_DATA_SHARING_LEAVE_DIALOG_TITLE),
          std::move(body_text),
          l10n_util::GetStringUTF16(IDS_DATA_SHARING_LEAVE_DIALOG_CONFIRM)};
    }
  }
}

bool IsDialogSkippable(DeletionDialogController::DialogType type) {
  switch (type) {
    // Saved tab group dialogs are skippable.
    case DeletionDialogController::DialogType::DeleteSingle:
    case DeletionDialogController::DialogType::UngroupSingle:
    case DeletionDialogController::DialogType::RemoveTabAndDelete:
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return true;
    }
    // Shared tab group dialogs aren't skippable.
    case DeletionDialogController::DialogType::DeleteSingleShared:
    case DeletionDialogController::DialogType::CloseTabAndKeepOrLeaveGroup:
    case DeletionDialogController::DialogType::CloseTabAndKeepOrDeleteGroup:
    case DeletionDialogController::DialogType::LeaveGroup: {
      return false;
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
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnDelete);
    }
    case DeletionDialogController::DialogType::UngroupSingle: {
      return pref_service->GetBoolean(
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnUngroup);
    }
    case DeletionDialogController::DialogType::RemoveTabAndDelete: {
      return pref_service->GetBoolean(
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnRemoveTab);
    }
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return pref_service->GetBoolean(
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnCloseTab);
    }
    // Shared tab groups dialogs aren't skippable.
    case DeletionDialogController::DialogType::DeleteSingleShared:
    case DeletionDialogController::DialogType::CloseTabAndKeepOrLeaveGroup:
    case DeletionDialogController::DialogType::CloseTabAndKeepOrDeleteGroup:
    case DeletionDialogController::DialogType::LeaveGroup: {
      return false;
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
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnDelete,
          new_value);
    }
    case DeletionDialogController::DialogType::UngroupSingle: {
      return pref_service->SetBoolean(
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnUngroup,
          new_value);
    }
    case DeletionDialogController::DialogType::RemoveTabAndDelete: {
      return pref_service->SetBoolean(
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnRemoveTab,
          new_value);
    }
    case DeletionDialogController::DialogType::CloseTabAndDelete: {
      return pref_service->SetBoolean(
          saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnCloseTab,
          new_value);
    }
    // Shared tab group dialogs aren't skippable.
    case DeletionDialogController::DialogType::DeleteSingleShared:
    case DeletionDialogController::DialogType::CloseTabAndKeepOrLeaveGroup:
    case DeletionDialogController::DialogType::CloseTabAndKeepOrDeleteGroup:
    case DeletionDialogController::DialogType::LeaveGroup: {
      // We should never try to set the skip pref for these dialog types.
      NOTREACHED();
    }
  }
}

// Keep type dialogs don't let the user cancel their action; instead, they
// choose whether the group should stick around or go away.
bool IsDialogKeepType(DeletionDialogController::DialogType type) {
  return type == DeletionDialogController::DialogType::
                     CloseTabAndKeepOrDeleteGroup ||
         type ==
             DeletionDialogController::DialogType::CloseTabAndKeepOrLeaveGroup;
}

}  // anonymous namespace

// DialogMetadata
DeletionDialogController::DialogMetadata::DialogMetadata(
    DialogType type,
    int closing_group_count,
    bool closing_multiple_tabs)
    : type(type),
      closing_group_count(closing_group_count),
      closing_multiple_tabs(closing_multiple_tabs) {}

DeletionDialogController::DialogMetadata::DialogMetadata(DialogType type)
    : type(type) {}

DeletionDialogController::DialogMetadata::~DialogMetadata() = default;

// DialogState
DeletionDialogController::DialogState::DialogState(
    DialogType type_,
    ui::DialogModel* dialog_model_,
    base::OnceCallback<void(DeletionDialogTiming)> callback_,
    std::optional<base::OnceCallback<void()>> keep_groups_)
    : type(type_),
      dialog_model(dialog_model_),
      callback(std::move(callback_)),
      keep_groups(std::move(keep_groups_)) {}

DeletionDialogController::DialogState::~DialogState() = default;

DeletionDialogController::DeletionDialogController(
    BrowserWindowInterface* browser,
    Profile* profile,
    TabStripModel* tab_strip_model)
    : profile_(CHECK_DEREF(profile)),
      show_dialog_model_fn_(base::BindRepeating(
          &DeletionDialogController::CreateDialogFromBrowser,
          base::Unretained(this),
          browser)),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)) {
  tab_strip_model_->AddObserver(this);
}

DeletionDialogController::DeletionDialogController(
    BrowserWindowInterface* browser,
    Profile* profile,
    TabStripModel* tab_strip_model,
    ShowDialogModelCallback show_dialog_model_fn)
    : profile_(CHECK_DEREF(profile)),
      show_dialog_model_fn_(show_dialog_model_fn),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)) {}

DeletionDialogController::~DeletionDialogController() = default;

bool DeletionDialogController::CanShowDialog() const {
  return !IsShowingDialog();
}

bool DeletionDialogController::IsShowingDialog() const {
  return !!state_;
}

void DeletionDialogController::CreateDialogFromBrowser(
    BrowserWindowInterface* browser,
    std::unique_ptr<ui::DialogModel> dialog_model) {
  widget_ = chrome::ShowBrowserModal(browser->GetBrowserForMigrationOnly(),
                                     std::move(dialog_model));
}

bool DeletionDialogController::MaybeShowDialog(
    const DialogMetadata& metadata,
    base::OnceCallback<void(DeletionDialogTiming)> callback,
    std::optional<base::OnceCallback<void()>> keep_groups) {
  if (IsDialogKeepType(metadata.type)) {
    CHECK(keep_groups.has_value());
  } else {
    CHECK(!keep_groups.has_value());
  }

  if (!CanShowDialog()) {
    return false;
  }

  if (IsDialogSkippedByUserSettings(GetProfile(), metadata.type)) {
    std::move(callback).Run(DeletionDialogTiming::Synchronous);
    return false;
  }

  std::unique_ptr<ui::DialogModel> dialog_model = BuildDialogModel(metadata);

  state_.emplace(metadata.type, dialog_model.get(), std::move(callback),
                 std::move(keep_groups));

  show_dialog_model_fn_.Run(std::move(dialog_model));
  return true;
}

void DeletionDialogController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (widget_) {
    widget_->Close();
  }
}

void DeletionDialogController::SetPrefsPreventShowingDialogForTesting(
    bool should_prevent_dialog) {
  auto* prefs = profile_->GetPrefs();
  prefs->SetBoolean(
      saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnDelete,
      should_prevent_dialog);
  prefs->SetBoolean(
      saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnUngroup,
      should_prevent_dialog);
  prefs->SetBoolean(
      saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnRemoveTab,
      should_prevent_dialog);
  prefs->SetBoolean(
      saved_tab_groups::prefs::kTabGroupsDeletionSkipDialogOnCloseTab,
      should_prevent_dialog);
}

void DeletionDialogController::OnDialogOk() {
  if (state_->dialog_model &&
      state_->dialog_model->HasField(kDeletionDialogDontAskCheckboxId) &&
      state_->dialog_model
          ->GetCheckboxByUniqueId(kDeletionDialogDontAskCheckboxId)
          ->is_checked()) {
    SetSkipDialogForType(GetProfile(), state_->type, true);
  }
  if (IsDialogKeepType(state_->type)) {
    std::move(state_->keep_groups.value()).Run();
  }
  std::move(state_->callback).Run(DeletionDialogTiming::Asynchronous);
  state_.reset();
}

void DeletionDialogController::OnDialogCancel() {
  if (IsDialogKeepType(state_->type)) {
    std::move(state_->callback).Run(DeletionDialogTiming::Asynchronous);
  }
  state_.reset();
}

std::unique_ptr<ui::DialogModel> DeletionDialogController::BuildDialogModel(
    const DialogMetadata& metadata) {
  DialogText strings = GetDialogText(GetProfile(), metadata);

  ui::DialogModel::Button::Params cancel_button_params;
  cancel_button_params.SetEnabled(true).SetId(kDeletionDialogCancelButtonId);
  if (strings.cancel_text.has_value()) {
    cancel_button_params.SetLabel(strings.cancel_text.value());
  }

  ui::DialogModel::Builder dialog_builder = ui::DialogModel::Builder();
  dialog_builder.SetTitle(strings.title)
      .AddParagraph(ui::DialogModelLabel(strings.body))
      .AddCancelButton(base::BindOnce(&DeletionDialogController::OnDialogCancel,
                                      base::Unretained(this)),
                       cancel_button_params)
      .AddOkButton(base::BindOnce(&DeletionDialogController::OnDialogOk,
                                  base::Unretained(this)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(strings.ok_text)
                       .SetEnabled(true)
                       .SetId(kDeletionDialogOkButtonId))
      .SetCloseActionCallback(base::BindOnce(
          [](DeletionDialogController* dialog_controller) {
            dialog_controller->state_.reset();
          },
          base::Unretained(this)))
      .SetDialogDestroyingCallback(base::BindOnce(
          [](DeletionDialogController* dialog_controller) {
            dialog_controller->widget_ = nullptr;
            dialog_controller->state_.reset();
          },
          base::Unretained(this)));
  if (IsDialogSkippable(metadata.type)) {
    dialog_builder.AddCheckbox(
        kDeletionDialogDontAskCheckboxId,
        ui::DialogModelLabel(l10n_util::GetStringUTF16(kDontAskId)));
  }
  return dialog_builder.Build();
}

Profile* DeletionDialogController::GetProfile() {
  return &profile_.get();
}

}  // namespace tab_groups
