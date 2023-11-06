// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_manager_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"

namespace {
// Passes the autofill `text` back into the `field` the dialog was opened on.
// Called upon insertion.
void FillTextWithAutofill(base::WeakPtr<autofill::AutofillManager> manager,
                          autofill::FieldGlobalId field,
                          const std::u16string& text) {
  // TODO(crbug.com/1331312): Replace this call by FillOrPreviewField.
  if (manager) {
    manager->driver().ApplyFieldAction(
        autofill::mojom::ActionPersistence::kFill,
        autofill::mojom::TextReplacement::kReplaceSelection, field, text);
  }
}

void GetBrowserFormHandler(autofill::FieldGlobalId field_id,
                           autofill::AutofillDriver* driver,
                           const std::optional<autofill::FormData>& form_data) {
  if (!form_data) {
    // TODO(b/305798770): replace with assert once form_data is always
    // populated.
    return;
  }
  const autofill::FormFieldData* form_field_data =
      form_data->FindFieldByGlobalId(field_id);
  if (!form_field_data) {
    // TODO(b/305798770): replace with assert once form_data is always
    // populated.
    return;
  }
  autofill::AutofillManager& manager = driver->GetAutofillManager();
  auto compose_callback =
      base::BindOnce(&FillTextWithAutofill, manager.GetWeakPtr(),
                     form_field_data->global_id());

  autofill::AutofillComposeDelegate* delegate =
      manager.client().GetComposeDelegate();
  delegate->OpenCompose(
      compose::ComposeManagerImpl::UiEntryPoint::kContextMenu, *form_field_data,
      manager.client().GetPopupScreenLocation(), std::move(compose_callback));
}

}  // namespace

namespace compose {

ComposeManagerImpl::ComposeManagerImpl(ComposeClient* client)
    : client_(*client) {}

ComposeManagerImpl::~ComposeManagerImpl() = default;

bool ComposeManagerImpl::ShouldOfferComposePopup(
    const autofill::FormFieldData& trigger_field) {
  return client_->ShouldTriggerPopup(trigger_field);
}

bool ComposeManagerImpl::HasSavedState(
    const autofill::FieldGlobalId& trigger_field_id) {
  // State is saved as a ComposeSession in the ComposeClient. A user can resume
  // where they left off in a field if the ComposeClient has a ComposeSession
  // for that field.
  return client_->HasSession(trigger_field_id);
}

bool ComposeManagerImpl::IsEnabled() const {
  return base::FeatureList::IsEnabled(features::kEnableCompose);
}

void ComposeManagerImpl::OpenComposeFromContextMenu(
    autofill::AutofillDriver* driver,
    const autofill::FormRendererId form_renderer_id,
    const autofill::FieldRendererId field_renderer_id) {
  const autofill::LocalFrameToken frame_token = driver->GetFrameToken();
  autofill::FormGlobalId form_global_id = {frame_token, form_renderer_id};
  autofill::FieldGlobalId field_global_id = {frame_token, field_renderer_id};

  driver->ExtractForm(form_global_id,
                      base::BindOnce(&GetBrowserFormHandler, field_global_id));
}

void ComposeManagerImpl::OpenCompose(
    UiEntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<PopupScreenLocation> popup_screen_location,
    ComposeCallback callback) {
  CHECK(IsEnabled());
  if (ui_entry_point == UiEntryPoint::kContextMenu) {
    compose::LogComposeContextMenuCtr(
        compose::ComposeContextMenuCtrEvent::kComposeOpened);
  }
  client_->ShowComposeDialog(ui_entry_point, trigger_field,
                             popup_screen_location, std::move(callback));
}

}  // namespace compose
