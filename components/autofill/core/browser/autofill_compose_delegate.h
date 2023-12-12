// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMPOSE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMPOSE_DELEGATE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

struct FormFieldData;

// The interface for communication from //components/autofill to
// //components/compose.
//
// In general, Compose uses Autofill as a platform/API: Compose is informed
// about certain renderer events (e.g. user focus on an appropriate textfield)
// and may choose to trigger Autofill to field the field.
// Therefore //components/compose should depend on //components/autofill. To
// still allow communication from //components/autofill to //components/compose,
// this interface exists and is injected via `AutofillClient`.
class AutofillComposeDelegate {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  virtual ~AutofillComposeDelegate() = default;

  // Ui entry points for the compose offer.
  enum class UiEntryPoint {
    kAutofillPopup,
    kContextMenu,
  };
  // Returns whether the compose popup is available for this `trigger_field`.
  virtual bool ShouldOfferComposePopup(const FormFieldData& trigger_field) = 0;

  // Returns whether the `trigger_field_id` has an existing state saved for
  // `trigger_field_id`. Saved state allows the user to return to a field and
  // resume where they left off.
  virtual bool HasSavedState(const FieldGlobalId& trigger_field_id) = 0;

  // Opens the Compose UI from the `ui_entry_point` given the 'driver',
  // 'form_renderer_id', and 'field_renderer_id'.
  virtual void OpenCompose(autofill::AutofillDriver& driver,
                           autofill::FormRendererId form_renderer_id,
                           autofill::FieldRendererId field_renderer_id,
                           UiEntryPoint ui_entry_point) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMPOSE_DELEGATE_H_
