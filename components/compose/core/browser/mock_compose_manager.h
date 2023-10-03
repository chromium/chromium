// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_MOCK_COMPOSE_MANAGER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_MOCK_COMPOSE_MANAGER_H_

#include "components/compose/core/browser/compose_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace compose {

class MockComposeManager : public ComposeManager {
 public:
  MockComposeManager();
  ~MockComposeManager() override;

  MOCK_METHOD(bool,
              ShouldOfferCompose,
              (TriggerMethod, const autofill::FormFieldData&),
              (override));
  MOCK_METHOD(void, OpenCompose, (ComposeCallback), (override));
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_MOCK_COMPOSE_MANAGER_H_
