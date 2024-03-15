// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/compose_utils.h"
#include "components/compose/core/browser/config.h"

namespace {

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
                           autofill::PopupItemId::kCompose);
}

}  // namespace

namespace compose {

ComposeManagerImpl::ComposeManagerImpl(ComposeClient* client)
    : client_(*client) {}

ComposeManagerImpl::~ComposeManagerImpl() = default;

bool ComposeManagerImpl::ShouldOfferComposePopup(
    const autofill::FormFieldData& trigger_field,
    autofill::AutofillSuggestionTriggerSource trigger_source) {
  return client_->ShouldTriggerPopup(trigger_field, trigger_source);
}

bool ComposeManagerImpl::HasSavedState(
    const autofill::FieldGlobalId& trigger_field_id) {
  // State is saved as a ComposeSession in the ComposeClient. A user can resume
  // where they left off in a field if the ComposeClient has a ComposeSession
  // for that field.
  return client_->HasSession(trigger_field_id);
}

void ComposeManagerImpl::OpenCompose(autofill::AutofillDriver& driver,
                                     autofill::FormGlobalId form_id,
                                     autofill::FieldGlobalId field_id,
                                     UiEntryPoint entry_point) {
  if (entry_point == UiEntryPoint::kContextMenu) {
    client_->getPageUkmTracker()->MenuItemClicked();
    compose::LogComposeContextMenuCtr(
        compose::ComposeContextMenuCtrEvent::kMenuItemClicked);
  }
  driver.ExtractForm(
      form_id,
      base::BindOnce(&ComposeManagerImpl::GetBrowserFormHandler,
                     weak_ptr_factory_.GetWeakPtr(), field_id, entry_point));
}

void ComposeManagerImpl::GetBrowserFormHandler(
    autofill::FieldGlobalId field_id,
    compose::ComposeManagerImpl::UiEntryPoint ui_entry_point,
    autofill::AutofillDriver* driver,
    const std::optional<autofill::FormData>& form_data) {
  if (!form_data) {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kAutofillFormDataNotFound);
    client_->getPageUkmTracker()->ShowDialogAbortedDueToMissingFormData();
    return;
  }
  const autofill::FormFieldData* form_field_data =
      form_data->FindFieldByGlobalId(field_id);
  if (!form_field_data) {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kAutofillFormFieldDataNotFound);
    client_->getPageUkmTracker()->ShowDialogAbortedDueToMissingFormFieldData();
    return;
  }

  autofill::AutofillManager& manager = driver->GetAutofillManager();
  ComposeCallback compose_callback;

  if (base::FeatureList::IsEnabled(compose::features::kComposeTextSelection) &&
      IsWordCountWithinBounds(base::UTF16ToUTF8(form_field_data->selected_text),
                              0, compose::GetComposeConfig().input_min_words)) {
    // Select all words.
    static_cast<autofill::BrowserAutofillManager*>(&manager)
        ->FillOrPreviewField(autofill::mojom::ActionPersistence::kFill,
                             autofill::mojom::FieldActionType::kSelectAll,
                             form_data.value(), *form_field_data, u"",
                             autofill::PopupItemId::kCompose);

    // Update form_field_data to use the newly selected text.
    autofill::FormFieldData updated_form_field_data =
        autofill::FormFieldData(*form_field_data);

    updated_form_field_data.selected_text = form_field_data->value;

    form_field_data = &updated_form_field_data;
  }

  compose_callback = base::BindOnce(&FillTextWithAutofill, manager.GetWeakPtr(),
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

}  // namespace compose
