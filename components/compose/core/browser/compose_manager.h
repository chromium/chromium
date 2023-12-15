// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"

namespace compose {

// The interface for embedder-independent, tab-specific compose logic.
class ComposeManager : public autofill::AutofillComposeDelegate {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  // TODO(b/300325327): Add non-Autofill specific methods.
  // Opens the Compose UI. `ui_entry_point` and `trigger_field` describe the
  // field on which Compose was triggered. `popup_screen_location` contains the
  // location (and arrow position) of the currently open popup bubble (if there
  // is one) and `callback` is the response callback to Autofill.
  virtual void OpenComposeWithFormFieldData(
      UiEntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field,
      std::optional<autofill::AutofillClient::PopupScreenLocation>
          popup_screen_location,
      ComposeCallback callback) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
