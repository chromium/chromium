// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_manager.h"

namespace compose {

class ComposeManagerImpl : public ComposeManager {
 public:
  using PopupScreenLocation = autofill::AutofillClient::PopupScreenLocation;

  explicit ComposeManagerImpl(ComposeClient* client);
  ComposeManagerImpl(const ComposeManagerImpl&) = delete;
  ComposeManagerImpl& operator=(const ComposeManagerImpl&) = delete;
  ~ComposeManagerImpl() override;

  // AutofillComposeDelegate
  bool ShouldOfferComposePopup(
      const autofill::FormFieldData& trigger_field) override;
  bool HasSavedState(const autofill::FieldGlobalId& trigger_field_id) override;
  void OpenCompose(UiEntryPoint ui_entry_point,
                   const autofill::FormFieldData& trigger_field,
                   std::optional<PopupScreenLocation> popup_screen_location,
                   ComposeCallback callback) override;

  // ComposeManager
  void OpenComposeFromContextMenu(
      autofill::AutofillDriver* driver,
      const autofill::FormRendererId form_renderer_id,
      const autofill::FieldRendererId field_renderer_id) override;

 private:
  bool IsEnabled() const;

  // A raw reference to the client, which owns `this` and therefore outlives it.
  const raw_ref<ComposeClient> client_;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
