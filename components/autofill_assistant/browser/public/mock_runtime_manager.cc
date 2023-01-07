// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"

namespace autofill_assistant {
MockRuntimeManager::MockRuntimeManager() = default;
MockRuntimeManager::~MockRuntimeManager() = default;

base::WeakPtr<RuntimeManager> MockRuntimeManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_assistant
