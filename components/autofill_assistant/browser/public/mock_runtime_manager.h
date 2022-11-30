// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_RUNTIME_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_RUNTIME_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockRuntimeManager : public RuntimeManager {
 public:
  MockRuntimeManager();
  virtual ~MockRuntimeManager();

  MOCK_METHOD1(AddObserver, void(RuntimeObserver*));
  MOCK_METHOD1(RemoveObserver, void(RuntimeObserver*));
  MOCK_CONST_METHOD0(GetState, UIState());
  MOCK_METHOD1(SetUIState, void(UIState));

  base::WeakPtr<RuntimeManager> GetWeakPtr() final;

 private:
  base::WeakPtrFactory<MockRuntimeManager> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_RUNTIME_MANAGER_H_
