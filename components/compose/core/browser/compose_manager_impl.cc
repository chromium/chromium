// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/compose_utils.h"
#include "components/compose/core/browser/config.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace compose {

namespace {

using autofill::AutofillDriver;
using autofill::AutofillSuggestionTriggerSource;
using autofill::FieldGlobalId;
using autofill::FormGlobalId;
using autofill::Suggestion;
using autofill::SuggestionType;

// Passes the autofill `text` back into the `field` the dialog was opened on.
// Called upon insertion.
void FillTextWithAutofill(base::WeakPtr<autofill::AutofillManager> manager,
                          const autofill::FormData& form,
                          const autofill::FormFieldData& field,
                          const std::u16string& text) {
  std::u16string trimmed_text;
  base::TrimString(text, u" \t\n\r\f\v", &trimmed_text);
  if (!manager) {
    return;
  }
  static_cast<autofill::BrowserAutofillManager*>(manager.get())
      ->FillOrPreviewField(autofill::mojom::ActionPersistence::kFill,
                           autofill::mojom::FieldActionType::kReplaceSelection,
                           form, field, trimmed_text,
                           SuggestionType::kComposeResumeNudge,
                           /*field_type_used=*/std::nullopt);
}

}  // namespace

ComposeManagerImpl::ComposeManagerImpl(ComposeClient* client)
    : client_(*client) {}

ComposeManagerImpl::~ComposeManagerImpl() = default;

void ComposeManagerImpl::OpenCompose(AutofillDriver& driver,
                                     FormGlobalId form_id,
                                     FieldGlobalId field_id,
                                     UiEntryPoint entry_point) {
  if (entry_point == UiEntryPoint::kContextMenu) {
    client_->GetPageUkmTracker()->MenuItemClicked();
    LogComposeContextMenuCtr(ComposeContextMenuCtrEvent::kMenuItemClicked);
  }
  driver.ExtractForm(
      form_id,
      base::BindOnce(&ComposeManagerImpl::OpenComposeWithUpdatedSelection,
                     weak_ptr_factory_.GetWeakPtr(), field_id, entry_point));
}

void ComposeManagerImpl::OpenComposeWithUpdatedSelection(
    FieldGlobalId field_id,
    ComposeManagerImpl::UiEntryPoint ui_entry_point,
    AutofillDriver* driver,
    const std::optional<autofill::FormData>& form_data) {
  if (!form_data) {
    LogOpenComposeDialogResult(
        OpenComposeDialogResult::kAutofillFormDataNotFound);
    client_->GetPageUkmTracker()->ShowDialogAbortedDueToMissingFormData();
    return;
  }

  const autofill::FormFieldData* form_field_data =
      form_data->FindFieldByGlobalId(field_id);
  if (!form_field_data) {
    LogOpenComposeDialogResult(
        OpenComposeDialogResult::kAutofillFormFieldDataNotFound);
    client_->GetPageUkmTracker()->ShowDialogAbortedDueToMissingFormFieldData();
    return;
  }

  if (base::FeatureList::IsEnabled(features::kComposeTextSelection) &&
      IsWordCountWithinBounds(
          base::UTF16ToUTF8(form_field_data->selected_text()), 0, 1)) {
    // Select all words. Consecutive calls using the same message pipe
    // should complete in the same order it's received.
    // Therefore, we can safely assume that the text selection will complete
    // before the subsequent extract form call without race conditions.
    driver->ApplyFieldAction(autofill::mojom::FieldActionType::kSelectAll,
                             autofill::mojom::ActionPersistence::kFill,
                             field_id, u"");

    // Re-extract form to update to the newly selected text.
    driver->ExtractForm(
        form_data->global_id(),
        base::BindOnce(&ComposeManagerImpl::OpenComposeWithFormData,
                       weak_ptr_factory_.GetWeakPtr(), field_id,
                       ui_entry_point));
    LogComposeSelectAllStatus(ComposeSelectAllStatus::kSelectedAll);
    return;
  }
  OpenComposeWithFormData(field_id, ui_entry_point, driver, form_data);
  LogComposeSelectAllStatus(ComposeSelectAllStatus::kNoSelectAll);
}

void ComposeManagerImpl::OpenComposeWithFormData(
    FieldGlobalId field_id,
    ComposeManagerImpl::UiEntryPoint ui_entry_point,
    AutofillDriver* driver,
    const std::optional<autofill::FormData>& form_data) {
  if (!form_data) {
    LogOpenComposeDialogResult(
        OpenComposeDialogResult::kAutofillFormDataNotFoundAfterSelectAll);
    client_->GetPageUkmTracker()->ShowDialogAbortedDueToMissingFormData();
    return;
  }

  const autofill::FormFieldData* form_field_data =
      form_data->FindFieldByGlobalId(field_id);
  if (!form_field_data) {
    LogOpenComposeDialogResult(
        OpenComposeDialogResult::kAutofillFormFieldDataNotFound);
    client_->GetPageUkmTracker()->ShowDialogAbortedDueToMissingFormFieldData();
    return;
  }

  autofill::AutofillManager& manager = driver->GetAutofillManager();
  ComposeCallback compose_callback =
      base::BindOnce(&FillTextWithAutofill, manager.GetWeakPtr(),
                     form_data.value(), *form_field_data);

  OpenComposeWithFormFieldData(ui_entry_point, *form_field_data,
                               manager.client().GetPopupScreenLocation(),
                               std::move(compose_callback));
}

void ComposeManagerImpl::OpenComposeWithFormFieldData(
    UiEntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<PopupScreenLocation> popup_screen_location,
    ComposeCallback callback) {
  client_->ShowComposeDialog(ui_entry_point, trigger_field,
                             popup_screen_location, std::move(callback));
}

std::optional<Suggestion> ComposeManagerImpl::GetSuggestion(
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!client_->ShouldTriggerPopup(form, field, trigger_source)) {
    return std::nullopt;
  }
  std::u16string suggestion_text;
  std::u16string label_text;
  SuggestionType type;
  // State is saved as a `ComposeSession` in the `ComposeClient`. A user can
  // resume where they left off in a field if the `ComposeClient` has a
  // `ComposeSession` for that field.
  const bool has_session = client_->HasSession(field.global_id());
  if (has_session) {
    // The nudge text indicates that the user can resume where they left off in
    // the Compose dialog.
    suggestion_text =
        l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_SAVED_TEXT);
    label_text = l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_SAVED_LABEL);
    type = trigger_source ==
                   AutofillSuggestionTriggerSource::kComposeDialogLostFocus
               ? SuggestionType::kComposeSavedStateNotification
               : SuggestionType::kComposeResumeNudge;
  } else {
    // Text for a new Compose session.
    suggestion_text =
        l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_MAIN_TEXT);
    label_text = l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_LABEL);
    type = SuggestionType::kComposeProactiveNudge;
  }
  Suggestion suggestion(std::move(suggestion_text));
  suggestion.type = type;
  suggestion.icon = Suggestion::Icon::kPenSpark;
  // Add footer label if not using compact ui.
  if (!GetComposeConfig().proactive_nudge_compact_ui) {
    suggestion.labels = {{Suggestion::Text(std::move(label_text))}};
  }

  if (!has_session &&
      base::FeatureList::IsEnabled(features::kEnableComposeProactiveNudge)) {
    // Add compose child suggestions
    Suggestion never_show_on_site = Suggestion(
        l10n_util::GetStringUTF16(
            IDS_COMPOSE_DONT_SHOW_ON_THIS_SITE_CHILD_SUGGESTION_TEXT),
        SuggestionType::kComposeNeverShowOnThisSiteAgain);
    Suggestion disable =
        Suggestion(l10n_util::GetStringUTF16(
                       IDS_COMPOSE_DISABLE_HELP_ME_WRITE_CHILD_SUGGESTION_TEXT),
                   SuggestionType::kComposeDisable);
    Suggestion go_to_settings =
        Suggestion(l10n_util::GetStringUTF16(
                       IDS_COMPOSE_GO_TO_SETTINGS_CHILD_SUGGESTION_TEXT),
                   SuggestionType::kComposeGoToSettings);
    suggestion.children = {std::move(never_show_on_site), std::move(disable),
                           std::move(go_to_settings)};
  }

  return suggestion;
}

void ComposeManagerImpl::NeverShowComposeForOrigin(const url::Origin& origin) {
  client_->AddSiteToNeverPromptList(origin);
  LogComposeProactiveNudgeCtr(ComposeProactiveNudgeCtrEvent::kUserDisabledSite);
  client_->GetPageUkmTracker()->ProactiveNudgeDisabledForSite();
}

void ComposeManagerImpl::DisableCompose() {
  client_->DisableProactiveNudge();
  LogComposeProactiveNudgeCtr(
      ComposeProactiveNudgeCtrEvent::kUserDisabledProactiveNudge);
  client_->GetPageUkmTracker()->ProactiveNudgeDisabledGlobally();
}

void ComposeManagerImpl::GoToSettings() {
  client_->OpenProactiveNudgeSettings();
  LogComposeProactiveNudgeCtr(ComposeProactiveNudgeCtrEvent::kOpenSettings);
}

bool ComposeManagerImpl::ShouldAnchorNudgeOnCaret() {
  return GetComposeConfig().is_nudge_shown_at_cursor;
}

}  // namespace compose
