// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/common/form_field_data.h"

namespace compose {

class ComposeManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
class ComposeClient {
 public:
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  virtual ~ComposeClient() = default;

  // Returns the `ComposeManager` associated with this client.
  virtual ComposeManager& GetManager() = 0;

  // Returns whether the `trigger_field_id` has a session (i.e., state).
  virtual bool HasSession(const autofill::FieldGlobalId& trigger_field_id) = 0;

  virtual void ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field,
      std::optional<autofill::AutofillClient::PopupScreenLocation>
          popup_screen_location,
      ComposeCallback callback) = 0;
  virtual bool ShouldTriggerPopup(
      const autofill::FormFieldData& trigger_field) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_
