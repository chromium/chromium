// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_RUNTIME_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_RUNTIME_MANAGER_H_

#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockRuntimeManager : public RuntimeManagerImpl {
 public:
  MockRuntimeManager();
  ~MockRuntimeManager() override;

  MOCK_METHOD1(AddObserver, void(RuntimeObserver*));
  MOCK_METHOD1(RemoveObserver, void(RuntimeObserver*));
  MOCK_CONST_METHOD0(GetState, UIState());
  MOCK_METHOD1(SetUIState, void(UIState));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_
