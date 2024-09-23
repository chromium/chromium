// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_manager.h"

namespace url {
class Origin;
}  // namespace url

namespace compose {

class ComposeManagerImpl : public ComposeManager {
 public:
  using PopupScreenLocation = autofill::AutofillClient::PopupScreenLocation;

  explicit ComposeManagerImpl(ComposeClient* client);
  ComposeManagerImpl(const ComposeManagerImpl&) = delete;
  ComposeManagerImpl& operator=(const ComposeManagerImpl&) = delete;
  ~ComposeManagerImpl() override;

  // AutofillComposeDelegate
  void OpenCompose(autofill::AutofillDriver& driver,
                   autofill::FormGlobalId form_id,
                   autofill::FieldGlobalId field_id,
                   UiEntryPoint ui_entry_point) override;

  // ComposeManager
  void OpenComposeWithFormFieldData(
      UiEntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field,
      std::optional<PopupScreenLocation> popup_screen_location,
      ComposeCallback callback) override;
  std::optional<autofill::Suggestion> GetSuggestion(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      autofill::AutofillSuggestionTriggerSource trigger_source) override;
  void NeverShowComposeForOrigin(const url::Origin& origin) override;
  void DisableCompose() override;
  void GoToSettings() override;
  bool ShouldAnchorNudgeOnCaret() override;

 private:
  bool IsEnabled() const;
  void OpenComposeWithFormData(
      autofill::FieldGlobalId field_id,
      compose::ComposeManagerImpl::UiEntryPoint ui_entry_point,
      autofill::AutofillDriver* driver,
      const std::optional<autofill::FormData>& form_data);
  void OpenComposeWithUpdatedSelection(
      autofill::FieldGlobalId field_id,
      compose::ComposeManagerImpl::UiEntryPoint ui_entry_point,
      autofill::AutofillDriver* driver,
      const std::optional<autofill::FormData>& form_data);

  // A raw reference to the client, which owns `this` and therefore outlives it.
  const raw_ref<ComposeClient> client_;

  base::WeakPtrFactory<ComposeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
