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
#include "components/compose/core/browser/compose_metrics.h"

namespace compose {

class ComposeManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
class ComposeClient {
 public:
  using FieldIdentifier =
      std::pair<autofill::FieldGlobalId, autofill::FormGlobalId>;
  // The callback to Autofill. When run, it fills the passed string into the
  // form field on which it was triggered.
  using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

  virtual ~ComposeClient() = default;

  // Returns the `ComposeManager` associated with this client.
  virtual ComposeManager& GetManager() = 0;

  // Returns whether the `trigger_field_id` has a session (i.e., state).
  virtual bool HasSession(const autofill::FieldGlobalId& trigger_field_id) = 0;

  // Requests the presentation of the Compose dialog for the provided field.
  virtual void ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field,
      std::optional<autofill::AutofillClient::PopupScreenLocation>
          popup_screen_location,
      ComposeCallback callback) = 0;

  // Checks if the popup (aka nudge) should be presented for the provided field.
  virtual bool ShouldTriggerPopup(
      const autofill::FormData& form_data,
      const autofill::FormFieldData& trigger_field,
      autofill::AutofillSuggestionTriggerSource trigger_source) = 0;

  // Getter for the PageUkmTracker instance for the currently loaded page.
  virtual PageUkmTracker* GetPageUkmTracker() = 0;

  // Disable the global preference controlling the proactive nudge.
  virtual void DisableProactiveNudge() = 0;

  // Open the "Offer writing help" settings page in a new active tab.
  virtual void OpenProactiveNudgeSettings() = 0;

  // Add `origin` to the preference managing sites on which the proactive nudge
  // is disabled.
  virtual void AddSiteToNeverPromptList(const url::Origin& origin) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_CLIENT_H_
