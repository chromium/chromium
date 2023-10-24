// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_MOCK_COMPOSE_MANAGER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_MOCK_COMPOSE_MANAGER_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/compose/core/browser/compose_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace compose {

class MockComposeManager : public ComposeManager {
 public:
  MockComposeManager();
  ~MockComposeManager() override;

  // AutofillComposeDelegate:
  MOCK_METHOD(bool,
              ShouldOfferComposePopup,
              (const autofill::FormFieldData&),
              (override));
  MOCK_METHOD(void,
              OpenCompose,
              (UiEntryPoint,
               const autofill::FormFieldData&,
               std::optional<autofill::AutofillClient::PopupScreenLocation>,
               ComposeCallback),
              (override));
  MOCK_METHOD(bool,
              HasSavedState,
              (const autofill::FieldGlobalId&),
              (override));

  // ComposeManager:
  MOCK_METHOD(bool, ShouldOfferComposeContextMenu, (), (override));
  MOCK_METHOD(void,
              OpenComposeFromContextMenu,
              (const autofill::LocalFrameToken,
               const autofill::FieldRendererId,
               const gfx::Point anchor),
              (override));
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_MOCK_COMPOSE_MANAGER_H_
