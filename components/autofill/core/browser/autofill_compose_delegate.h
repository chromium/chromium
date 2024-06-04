// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMPOSE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMPOSE_DELEGATE_H_

#include <optional>

#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {

class AutofillDriver;
class FormFieldData;

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
  virtual ~AutofillComposeDelegate() = default;

  // Ui entry points for the compose offer.
  enum class UiEntryPoint {
    kAutofillPopup,
    kContextMenu,
  };

  // Opens the Compose UI from the `ui_entry_point` given the 'driver',
  // 'form_id', and 'field_id'.
  virtual void OpenCompose(AutofillDriver& driver,
                           FormGlobalId form_id,
                           FieldGlobalId field_id,
                           UiEntryPoint ui_entry_point) = 0;

  // Disables the compose feature for `origin`.
  virtual void NeverShowComposeForOrigin(const url::Origin& origin) = 0;

  // Disables the compose feature everywhere.
  virtual void DisableCompose() = 0;

  // Navigates the user to the compose settings page.
  virtual void GoToSettings() = 0;

  // Returns a suggestion if the compose service is available for
  // `field`.
  virtual std::optional<autofill::Suggestion> GetSuggestion(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      autofill::AutofillSuggestionTriggerSource trigger_source) = 0;

  // Whether the Autofill nudge should be anchored on the caret or on the
  // triggering field.
  virtual bool ShouldAnchorNudgeOnCaret() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMPOSE_DELEGATE_H_
