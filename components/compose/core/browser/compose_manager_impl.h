// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
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

  // ComposeManager:
  bool ShouldOfferCompose(
      UiEntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field) override;
  void OpenCompose(UiEntryPoint ui_entry_point,
                   const autofill::FormFieldData& trigger_field,
                   std::optional<PopupScreenLocation> popup_screen_location,
                   ComposeCallback callback) override;
  void OpenComposeFromContextMenu(
      const autofill::LocalFrameToken frame_token,
      const autofill::FieldRendererId field_renderer_id,
      const gfx::RectF bounds) override;

 private:
  bool IsEnabled() const;
  void ComposeTextForQuery(const ComposeClient::QueryParams& params);
  // TODO(b/305798770): Remove the following two methods once hooked up to the
  // mojo call. They are currently used to provide a reduced FormFieldData to
  // Compose.
  void OpenComposeFromContextMenuCallback(
      const autofill::FormFieldData form_field_data);
  void RequestFormFieldData(
      const autofill::LocalFrameToken frame_token,
      const autofill::FieldRendererId field_renderer_id,
      const gfx::RectF bounds,
      base::OnceCallback<void(autofill::FormFieldData)> callback);

  // A raw reference to the client, which owns `this` and therefore outlives it.
  const raw_ref<ComposeClient> client_;

  // A callback to Autofill that triggers filling the field.
  ComposeCallback callback_;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_IMPL_H_
